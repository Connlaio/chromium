// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/template_url_fetcher.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_parser.h"
#include "components/search_engines/template_url_service.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"

// RequestDelegate ------------------------------------------------------------
class TemplateURLFetcher::RequestDelegate : public net::URLFetcherDelegate {
 public:
  RequestDelegate(
      TemplateURLFetcher* fetcher,
      const base::string16& keyword,
      const GURL& osdd_url,
      const GURL& favicon_url,
      const URLFetcherCustomizeCallback& url_fetcher_customize_callback,
      const ConfirmAddSearchProviderCallback& confirm_add_callback,
      ProviderType provider_type);

  // net::URLFetcherDelegate:
  // If data contains a valid OSDD, a TemplateURL is created and added to
  // the TemplateURLService.
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  // URL of the OSDD.
  GURL url() const { return osdd_url_; }

  // Keyword to use.
  base::string16 keyword() const { return keyword_; }

  // The type of search provider being fetched.
  ProviderType provider_type() const { return provider_type_; }

 private:
  void OnLoaded();
  void AddSearchProvider();

  std::unique_ptr<net::URLFetcher> url_fetcher_;
  TemplateURLFetcher* fetcher_;
  std::unique_ptr<TemplateURL> template_url_;
  base::string16 keyword_;
  const GURL osdd_url_;
  const GURL favicon_url_;
  const ProviderType provider_type_;
  ConfirmAddSearchProviderCallback confirm_add_callback_;

  std::unique_ptr<TemplateURLService::Subscription> template_url_subscription_;

  DISALLOW_COPY_AND_ASSIGN(RequestDelegate);
};

TemplateURLFetcher::RequestDelegate::RequestDelegate(
    TemplateURLFetcher* fetcher,
    const base::string16& keyword,
    const GURL& osdd_url,
    const GURL& favicon_url,
    const URLFetcherCustomizeCallback& url_fetcher_customize_callback,
    const ConfirmAddSearchProviderCallback& confirm_add_callback,
    ProviderType provider_type)
    : url_fetcher_(
          net::URLFetcher::Create(osdd_url, net::URLFetcher::GET, this)),
      fetcher_(fetcher),
      keyword_(keyword),
      osdd_url_(osdd_url),
      favicon_url_(favicon_url),
      provider_type_(provider_type),
      confirm_add_callback_(confirm_add_callback) {
  TemplateURLService* model = fetcher_->template_url_service_;
  DCHECK(model);  // TemplateURLFetcher::ScheduleDownload verifies this.

  if (!model->loaded()) {
    // Start the model load and set-up waiting for it.
    template_url_subscription_ = model->RegisterOnLoadedCallback(
        base::Bind(&TemplateURLFetcher::RequestDelegate::OnLoaded,
                   base::Unretained(this)));
    model->Load();
  }

  if (!url_fetcher_customize_callback.is_null())
    url_fetcher_customize_callback.Run(url_fetcher_.get());

  url_fetcher_->SetRequestContext(fetcher->request_context_.get());
  url_fetcher_->Start();
}

void TemplateURLFetcher::RequestDelegate::OnLoaded() {
  template_url_subscription_.reset();
  if (!template_url_.get())
    return;
  AddSearchProvider();
  // WARNING: AddSearchProvider deletes us.
}

void TemplateURLFetcher::RequestDelegate::OnURLFetchComplete(
    const net::URLFetcher* source) {
  // Validation checks.
  // Make sure we can still replace the keyword, i.e. the fetch was successful.
  // If the OSDD file was loaded HTTP, we also have to check the response_code.
  // For other schemes, e.g. when the OSDD file is bundled with an extension,
  // the response_code is not applicable and should be -1. Also, ensure that
  // the returned information results in a valid search URL.
  std::string data;
  if (!source->GetStatus().is_success() ||
      ((source->GetResponseCode() != -1) &&
        (source->GetResponseCode() != 200)) ||
      !source->GetResponseAsString(&data)) {
    fetcher_->RequestCompleted(this);
    // WARNING: RequestCompleted deletes us.
    return;
  }

  template_url_.reset(TemplateURLParser::Parse(
      fetcher_->template_url_service_->search_terms_data(), false,
      data.data(), data.length(), NULL));
  if (!template_url_.get() ||
      !template_url_->url_ref().SupportsReplacement(
          fetcher_->template_url_service_->search_terms_data())) {
    fetcher_->RequestCompleted(this);
    // WARNING: RequestCompleted deletes us.
    return;
  }

  if (provider_type_ != AUTODETECTED_PROVIDER || keyword_.empty()) {
    // Use the parser-generated new keyword from the URL in the OSDD for the
    // non-autodetected case.  The existing |keyword_| was generated from the
    // URL that hosted the OSDD, which results in the wrong keyword when the
    // OSDD was located on a third-party site that has nothing in common with
    // search engine described by OSDD.
    keyword_ = template_url_->keyword();
    DCHECK(!keyword_.empty());
  }

  // Wait for the model to be loaded before adding the provider.
  if (!fetcher_->template_url_service_->loaded())
    return;
  AddSearchProvider();
  // WARNING: AddSearchProvider deletes us.
}

void TemplateURLFetcher::RequestDelegate::AddSearchProvider() {
  DCHECK(template_url_.get());
  DCHECK(!keyword_.empty());
  TemplateURLService* model = fetcher_->template_url_service_;
  DCHECK(model);
  DCHECK(model->loaded());

  TemplateURL* existing_url = NULL;
  if (model->CanAddAutogeneratedKeyword(keyword_, GURL(template_url_->url()),
                                        &existing_url)) {
    if (existing_url)
      model->Remove(existing_url);
  } else if (provider_type_ == AUTODETECTED_PROVIDER) {
    fetcher_->RequestCompleted(this);  // WARNING: Deletes us!
    return;
  }

  // The short name is what is shown to the user. We preserve original names
  // since it is better when generated keyword in many cases.
  TemplateURLData data(template_url_->data());
  data.SetKeyword(keyword_);
  data.originating_url = osdd_url_;

  // The page may have specified a URL to use for favicons, if not, set it.
  if (!data.favicon_url.is_valid())
    data.favicon_url = favicon_url_;

  switch (provider_type_) {
    case AUTODETECTED_PROVIDER:
      // Mark the keyword as replaceable so it can be removed if necessary.
      data.safe_for_autoreplace = true;
      model->Add(new TemplateURL(data));
      break;

    case EXPLICIT_PROVIDER:
      // Confirm addition and allow user to edit default choices. It's ironic
      // that only *non*-autodetected additions get confirmed, but the user
      // expects feedback that his action did something.
      // The source WebContents' delegate takes care of adding the URL to the
      // model, which takes ownership, or of deleting it if the add is
      // cancelled.
      confirm_add_callback_.Run(base::WrapUnique(new TemplateURL(data)));
      break;

    default:
      NOTREACHED();
      break;
  }

  fetcher_->RequestCompleted(this);
  // WARNING: RequestCompleted deletes us.
}

// TemplateURLFetcher ---------------------------------------------------------

TemplateURLFetcher::TemplateURLFetcher(
    TemplateURLService* template_url_service,
    net::URLRequestContextGetter* request_context)
    : template_url_service_(template_url_service),
      request_context_(request_context) {
}

TemplateURLFetcher::~TemplateURLFetcher() {
}

void TemplateURLFetcher::ScheduleDownload(
    const base::string16& keyword,
    const GURL& osdd_url,
    const GURL& favicon_url,
    const URLFetcherCustomizeCallback& url_fetcher_customize_callback,
    const ConfirmAddSearchProviderCallback& confirm_add_callback,
    ProviderType provider_type) {
  DCHECK(osdd_url.is_valid());

  // For a JS-added OSDD, the provided keyword is irrelevant because we will
  // generate a keyword later from the OSDD content.  For the autodetected case,
  // we need a valid keyword up front.
  if (provider_type == TemplateURLFetcher::AUTODETECTED_PROVIDER) {
    DCHECK(!keyword.empty());

    if (!template_url_service_->loaded()) {
      // We could try to set up a callback to this function again once the model
      // is loaded but since this is an auto-add case anyway, meh.
      template_url_service_->Load();
      return;
    }

    const TemplateURL* template_url =
        template_url_service_->GetTemplateURLForKeyword(keyword);
    if (template_url && (!template_url->safe_for_autoreplace() ||
                         template_url->originating_url() == osdd_url))
      return;
  }

  // Make sure we aren't already downloading this request.
  for (Requests::iterator i = requests_.begin(); i != requests_.end(); ++i) {
    if (((*i)->url() == osdd_url) ||
        ((provider_type == TemplateURLFetcher::AUTODETECTED_PROVIDER) &&
         ((*i)->keyword() == keyword)))
      return;
  }

  requests_.push_back(new RequestDelegate(
      this, keyword, osdd_url, favicon_url, url_fetcher_customize_callback,
      confirm_add_callback, provider_type));
}

void TemplateURLFetcher::RequestCompleted(RequestDelegate* request) {
  Requests::iterator i =
      std::find(requests_.begin(), requests_.end(), request);
  DCHECK(i != requests_.end());
  requests_.weak_erase(i);
  delete request;
}
