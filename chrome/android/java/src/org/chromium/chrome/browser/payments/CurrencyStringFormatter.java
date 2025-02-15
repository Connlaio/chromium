// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import java.text.DecimalFormatSymbols;
import java.util.Currency;
import java.util.Locale;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Formatter for currency strings that can be too large to parse into numbers.
 * https://w3c.github.io/browser-payment-api/specs/paymentrequest.html#currencyamount
 */
public class CurrencyStringFormatter {
    // Amount value pattern and capture group numbers.
    private static final String AMOUNT_VALUE_PATTERN = "^(-?)([0-9]+)(\\.([0-9]+))?$";
    private static final int OPTIONAL_NEGATIVE_GROUP = 1;
    private static final int DIGITS_BETWEEN_NEGATIVE_AND_PERIOD_GROUP = 2;
    private static final int DIGITS_AFTER_PERIOD_GROUP = 4;

    // Amount currency code pattern.
    private static final String AMOUNT_CURRENCY_CODE_PATTERN = "^[A-Z]{3}$";

    // Formatting constants.
    private static final int DIGIT_GROUPING_SIZE = 3;

    private final Pattern mAmountValuePattern;
    private final Pattern mAmountCurrencyCodePattern;

    /**
     * The symbol for the currency specified on the bill. For example, the symbol for "USD" is "$".
     */
    private final String mCurrencySymbol;

    /**
     * The number grouping separator for the current locale. For example, "," in US. 3-digit groups
     * are assumed.
     */
    private final char mGroupingSeparator;

    /**
     * The decimal separator for the current locale. For example, "." in US and "," in France.
     */
    private final char mMonetaryDecimalSeparator;

    /**
     * Builds the formatter for the given currency code and the current user locale.
     *
     * @param currencyCode The currency code. Most commonly, this follows ISO 4217 format: 3 upper
     *                     case ASCII letters. For example, "USD". Format is not restricted. Should
     *                     not be null.
     * @param userLocale User's current locale. Should not be null.
     */
    public CurrencyStringFormatter(String currencyCode, Locale userLocale) {
        assert currencyCode != null : "currencyCode should not be null";
        assert userLocale != null : "userLocale should not be null";

        mAmountValuePattern = Pattern.compile(AMOUNT_VALUE_PATTERN);
        mAmountCurrencyCodePattern = Pattern.compile(AMOUNT_CURRENCY_CODE_PATTERN);

        String currencySymbol;
        try {
            currencySymbol = Currency.getInstance(currencyCode).getSymbol();
        } catch (IllegalArgumentException e) {
            // The spec does not limit the currencies to official ISO 4217 currency code list, which
            // is used by java.util.Currency. For example, "BTX" (bitcoin) is not an official ISO
            // 4217 currency code, but is allowed by the spec.
            currencySymbol = currencyCode;
        }
        mCurrencySymbol = currencySymbol;

        // Use the symbols from user's current locale. For example, use "," for decimal separator in
        // France, even if paying in "USD".
        DecimalFormatSymbols symbols = new DecimalFormatSymbols(userLocale);
        mGroupingSeparator = symbols.getGroupingSeparator();
        mMonetaryDecimalSeparator = symbols.getMonetaryDecimalSeparator();
    }

    /**
     * Returns true if the amount value string is in valid format.
     *
     * @param amountValue The number to check for validity.
     * @return Whether the number is in valid format.
     */
    public boolean isValidAmountValue(String amountValue) {
        return amountValue != null && mAmountValuePattern.matcher(amountValue).matches();
    }

    /**
     * Returns true if the currency code string is in valid format.
     *
     * @param amountCurrencyCode The currency code to check for validity.
     * @return Whether the currency code is in valid format.
     */
    public boolean isValidAmountCurrencyCode(String amountCurrencyCode) {
        return amountCurrencyCode != null
                && mAmountCurrencyCodePattern.matcher(amountCurrencyCode).matches();
    }

    /**
     * Formats the currency string for display. Does not parse the string into a number, because it
     * might be too large. The number is formatted for the current locale and follows the symbol of
     * the currency code.
     *
     * @param amountValue The number to format. Should be in "^-?[0-9]+(\.[0-9]+)?$" format. Should
     *                    not be null.
     * @return The currency symbol followed by a space and the formatted number.
     */
    public String format(String amountValue) {
        assert amountValue != null : "amountValue should not be null";

        Matcher m = mAmountValuePattern.matcher(amountValue);

        // Required to capture the groups.
        boolean matches = m.matches();
        assert matches;

        StringBuilder result = new StringBuilder(mCurrencySymbol);
        result.append(" ");
        result.append(m.group(OPTIONAL_NEGATIVE_GROUP));
        int digitStart = result.length();

        result.append(m.group(DIGITS_BETWEEN_NEGATIVE_AND_PERIOD_GROUP));
        for (int i = result.length() - DIGIT_GROUPING_SIZE; i > digitStart;
                i -= DIGIT_GROUPING_SIZE) {
            result.insert(i, mGroupingSeparator);
        }

        result.append(mMonetaryDecimalSeparator);

        String decimals = m.group(DIGITS_AFTER_PERIOD_GROUP);
        int numberOfDecimals = 0;
        if (decimals != null) {
            numberOfDecimals = decimals.length();
            result.append(decimals);
        }

        for (int i = numberOfDecimals; i < 2; i++) {
            result.append("0");
        }

        return result.toString();
    }
}
