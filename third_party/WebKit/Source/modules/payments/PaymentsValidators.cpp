// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/payments/PaymentsValidators.h"

#include "bindings/core/v8/ScriptRegexp.h"
#include "wtf/text/StringImpl.h"

namespace blink {

bool PaymentsValidators::isValidCurrencyCodeFormat(const String& code, String* optionalErrorMessage)
{
    if (ScriptRegexp("^[A-Z]{3}$", TextCaseSensitive).match(code) == 0)
        return true;

    if (optionalErrorMessage)
        *optionalErrorMessage = "'" + code + "' is not a valid ISO 4217 currency code, should be 3 upper case letters [A-Z]";

    return false;
}

bool PaymentsValidators::isValidAmountFormat(const String& amount, String* optionalErrorMessage)
{
    if (ScriptRegexp("^-?[0-9]+(\\.[0-9]+)?$", TextCaseSensitive).match(amount) == 0)
        return true;

    if (optionalErrorMessage)
        *optionalErrorMessage = "'" + amount + "' is not a valid ISO 20022 CurrencyAnd30Amount";

    return false;
}

bool PaymentsValidators::isValidRegionCodeFormat(const String& code, String* optionalErrorMessage)
{
    if (ScriptRegexp("^[A-Z]{2}$", TextCaseSensitive).match(code) == 0)
        return true;

    if (optionalErrorMessage)
        *optionalErrorMessage = "'" + code + "' is not a valid ISO 3166 country code, should be 2 upper case letters [A-Z]";

    return false;
}

bool PaymentsValidators::isValidLanguageCodeFormat(const String& code, String* optionalErrorMessage)
{
    if (ScriptRegexp("^([a-z]{2,3})?$", TextCaseSensitive).match(code) == 0)
        return true;

    if (optionalErrorMessage)
        *optionalErrorMessage = "'" + code + "' is not a valid ISO 639 language code, should be 2-3 lower case letters [a-z]";

    return false;
}

bool PaymentsValidators::isValidScriptCodeFormat(const String& code, String* optionalErrorMessage)
{
    if (ScriptRegexp("^([A-Z][a-z]{3})?$", TextCaseSensitive).match(code) == 0)
        return true;

    if (optionalErrorMessage)
        *optionalErrorMessage = "'" + code + "' is not a valid ISO 15924 script code, should be an upper case letter [A-Z] followed by 3 lower case letters [a-z]";

    return false;
}

} // namespace blink
