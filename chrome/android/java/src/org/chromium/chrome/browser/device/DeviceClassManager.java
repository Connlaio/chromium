// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.device;

import android.content.Context;
import android.view.accessibility.AccessibilityManager;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.CommandLine;
import org.chromium.base.SysUtils;
import org.chromium.base.TraceEvent;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.ui.base.DeviceFormFactor;

/**
 * This class is used to turn on and off certain features for different types of
 * devices.
 */
public class DeviceClassManager {
    private static final int DISABLE_AUTO_HIDING_TOOLBAR_THRESHOLD = 800;

    private static DeviceClassManager sInstance;

    // Set of features that can be enabled/disabled
    private boolean mEnableSnapshots;
    private boolean mEnableLayerDecorationCache;
    private boolean mEnableAccessibilityLayout;
    private boolean mEnableAnimations;
    private boolean mEnablePrerendering;
    private boolean mEnableToolbarSwipe;
    private boolean mEnableToolbarSwipeInDocumentMode;
    private boolean mDisableDomainReliability;

    private final boolean mEnableFullscreen;
    private Boolean mDisableAutoHidingToolbar;

    private static DeviceClassManager getInstance() {
        if (sInstance == null) {
            sInstance = new DeviceClassManager();
        }
        return sInstance;
    }

    /**
     * The {@link DeviceClassManager} constructor should be self contained and
     * rely on system information and command line flags.
     */
    private DeviceClassManager() {
        // Device based configurations.
        if (SysUtils.isLowEndDevice()) {
            mEnableSnapshots = false;
            mEnableLayerDecorationCache = true;
            mEnableAccessibilityLayout = true;
            mEnableAnimations = false;
            mEnablePrerendering = false;
            mEnableToolbarSwipe = false;
            mDisableDomainReliability = true;
        } else {
            mEnableSnapshots = true;
            mEnableLayerDecorationCache = true;
            mEnableAccessibilityLayout = false;
            mEnableAnimations = true;
            mEnablePrerendering = true;
            mEnableToolbarSwipe = true;
            mDisableDomainReliability = false;
        }

        if (DeviceFormFactor.isTablet(ApplicationStatus.getApplicationContext())) {
            mEnableAccessibilityLayout = false;
        }

        // Flag based configurations.
        CommandLine commandLine = CommandLine.getInstance();
        mEnableAccessibilityLayout |= commandLine
                .hasSwitch(ChromeSwitches.ENABLE_ACCESSIBILITY_TAB_SWITCHER);
        mEnableFullscreen =
                !commandLine.hasSwitch(ChromeSwitches.DISABLE_FULLSCREEN);
        mEnableToolbarSwipeInDocumentMode =
                commandLine.hasSwitch(ChromeSwitches.ENABLE_TOOLBAR_SWIPE_IN_DOCUMENT_MODE);

        // Related features.
        if (mEnableAccessibilityLayout) {
            mEnableAnimations = false;
        }
    }

    /**
     * @return Whether or not we can take screenshots.
     */
    public static boolean enableSnapshots() {
        return getInstance().mEnableSnapshots;
    }

    /**
     * @return Whether or not we can use the layer decoration cache.
     */
    public static boolean enableLayerDecorationCache() {
        return getInstance().mEnableLayerDecorationCache;
    }

    /**
     * @return Whether or not should use the accessibility tab switcher.
     */
    public static boolean enableAccessibilityLayout() {
        return getInstance().mEnableAccessibilityLayout;
    }

    /**
     * @return Whether or not full screen is enabled.
     */
    public static boolean enableFullscreen() {
        return getInstance().mEnableFullscreen;
    }

    /**
     * @param context A {@link Context} instance.
     * @return        Whether or not we are showing animations.
     */
    public static boolean enableAnimations(Context context) {
        return getInstance().mEnableAnimations && !isAccessibilityModeEnabled(context);
    }

    /**
     * @return Whether or not prerendering is enabled.
     */
    public static boolean enablePrerendering() {
        return getInstance().mEnablePrerendering;
    }

    /**
     * @param isDocumentMode Whether or not chrome is in document mode.
     * @return Whether or not we can use the toolbar swipe.
     */
    public static boolean enableToolbarSwipe(boolean isDocumentMode) {
        return getInstance().mEnableToolbarSwipe
                && !(isDocumentMode && !getInstance().mEnableToolbarSwipeInDocumentMode);
    }

    /**
     * @return Whether or not to disable domain reliability.
     */
    public static boolean disableDomainReliability() {
        return getInstance().mDisableDomainReliability;
    }

    public static boolean isAccessibilityModeEnabled(Context context) {
        TraceEvent.begin("DeviceClassManager::isAccessibilityModeEnabled");
        AccessibilityManager manager = (AccessibilityManager)
                context.getSystemService(Context.ACCESSIBILITY_SERVICE);
        boolean enabled = manager != null && manager.isEnabled()
                && manager.isTouchExplorationEnabled();
        TraceEvent.end("DeviceClassManager::isAccessibilityModeEnabled");
        return enabled;
    }

    /**
     * @param context A {@link Context} instance.
     * @return Whether auto-hiding the toolbar is disabled.
     */
    public static boolean isAutoHidingToolbarDisabled(Context context) {
        if (getInstance().mDisableAutoHidingToolbar == null) {
            getInstance().mDisableAutoHidingToolbar = false;
            int smallestScreenDp = context.getResources().getConfiguration().smallestScreenWidthDp;
            if (smallestScreenDp >= DISABLE_AUTO_HIDING_TOOLBAR_THRESHOLD) {
                getInstance().mDisableAutoHidingToolbar = true;
            }
        }
        return getInstance().mDisableAutoHidingToolbar;
    }
}
