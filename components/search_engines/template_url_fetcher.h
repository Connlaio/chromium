// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_TEMPLATE_URL_FETCHER_H_
#define COMPONENTS_SEARCH_ENGINES_TEMPLATE_URL_FETCHER_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "base/strings/string16.h"
#include "components/keyed_service/core/keyed_service.h"

class GURL;
class TemplateURL;
class TemplateURLService;

namespace net {
class URLFetcher;
class URLRequestContextGetter;
}

// TemplateURLFetcher is responsible for downloading OpenSearch description
// documents, creating a TemplateURL from the OSDD, and adding the TemplateURL
// to the TemplateURLService. Downloading is done in the background.
//
class TemplateURLFetcher : public KeyedService {
 public:
  typedef base::Callback<void(
      net::URLFetcher* url_fetcher)> URLFetcherCustomizeCallback;
  typedef base::Callback<void(std::unique_ptr<TemplateURL> template_url)>
      ConfirmAddSearchProviderCallback;

  enum ProviderType {
    AUTODETECTED_PROVIDER,
    EXPLICIT_PROVIDER  // Supplied by Javascript.
  };

  // Creates a TemplateURLFetcher.
  TemplateURLFetcher(TemplateURLService* template_url_service,
                     net::URLRequestContextGetter* request_context);
  ~TemplateURLFetcher() override;

  // If TemplateURLFetcher is not already downloading the OSDD for osdd_url,
  // it is downloaded. If successful and the result can be parsed, a TemplateURL
  // is added to the TemplateURLService.
  //
  // If |provider_type| is AUTODETECTED_PROVIDER, |keyword| must be non-empty,
  // and if there's already a non-replaceable TemplateURL in the model for
  // |keyword|, or we're already downloading an OSDD for this keyword, no
  // download is started.  If |provider_type| is EXPLICIT_PROVIDER, |keyword| is
  // ignored.
  //
  // If |url_fetcher_customize_callback| is not null, it's run after a
  // URLFetcher is created. This callback can be used to set additional
  // parameters on the URLFetcher.
  void ScheduleDownload(
      const base::string16& keyword,
      const GURL& osdd_url,
      const GURL& favicon_url,
      const URLFetcherCustomizeCallback& url_fetcher_customize_callback,
      const ConfirmAddSearchProviderCallback& confirm_add_callback,
      ProviderType provider_type);

  // The current number of outstanding requests.
  int requests_count() const { return requests_.size(); }

 private:
  // A RequestDelegate is created to download each OSDD. When done downloading
  // RequestCompleted is invoked back on the TemplateURLFetcher.
  class RequestDelegate;
  friend class RequestDelegate;

  typedef ScopedVector<RequestDelegate> Requests;

  // Invoked from the RequestDelegate when done downloading.
  void RequestCompleted(RequestDelegate* request);

  TemplateURLService* template_url_service_;
  scoped_refptr<net::URLRequestContextGetter> request_context_;

  // In progress requests.
  Requests requests_;

  DISALLOW_COPY_AND_ASSIGN(TemplateURLFetcher);
};

#endif  // COMPONENTS_SEARCH_ENGINES_TEMPLATE_URL_FETCHER_H_
