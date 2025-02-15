// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-display' is the settings subpage for display settings.
 *
 * @group Chrome Settings Elements
 */

cr.define('settings.display', function() {
  var systemDisplayApi = /** @type {!SystemDisplay} */ (chrome.system.display);

  return {
    systemDisplayApi: systemDisplayApi,
  };
});

Polymer({
  is: 'settings-display',

  behaviors: [
    I18nBehavior,
  ],

  properties: {
    /**
     * Array of displays.
     * @type {!Array<!chrome.system.display.DisplayUnitInfo>}
     */
    displays: Array,

    /** Primary display id */
    primaryDisplayId: String,

    /**
     * Selected display
     * @type {!chrome.system.display.DisplayUnitInfo|undefined}
     */
    selectedDisplay: {type: Object, observer: 'selectedDisplayChanged_'},

    /** Maximum mode index value for slider. */
    maxModeIndex_: {type: Number, value: 0},

    /** Selected mode index value for slider. */
    selectedModeIndex_: {type: Number},

    /** Immediate selected mode index value for slider. */
    immediateSelectedModeIndex_: {type: Number, value: 0}
  },

  /**
   * Listener for chrome.system.display.onDisplayChanged events.
   * @type {function(void)|undefined}
   * @private
   */
  displayChangedListener_: undefined,

  /** @override */
  attached: function() {
    this.displayChangedListener_ = this.getDisplayInfo_.bind(this);
    settings.display.systemDisplayApi.onDisplayChanged.addListener(
        this.displayChangedListener_);
    this.getDisplayInfo_();
  },

  /** @override */
  detached: function() {
    if (this.displayChangedListener_) {
      settings.display.systemDisplayApi.onDisplayChanged.removeListener(
          this.displayChangedListener_);
    }
  },

  /** @private */
  getDisplayInfo_: function() {
    settings.display.systemDisplayApi.getInfo(
        this.updateDisplayInfo_.bind(this));
  },

  /**
   * @param {!chrome.system.display.DisplayUnitInfo} selectedDisplay
   * @return {number}
   * @private
   */
  getSelectedModeIndex_: function(selectedDisplay) {
    for (var i = 0; i < selectedDisplay.modes.length; ++i) {
      if (selectedDisplay.modes[i].isSelected)
        return i;
    }
    return 0;
  },

  /** @private */
  selectedDisplayChanged_: function() {
    // Set maxModeIndex first so that the slider updates correctly.
    if (this.selectedDisplay.modes.length == 0) {
      this.maxModeIndex_ = 0;
      this.selectedModeIndex_ = 0;
      return;
    }
    this.maxModeIndex_ = this.selectedDisplay.modes.length - 1;
    this.selectedModeIndex_ = this.getSelectedModeIndex_(this.selectedDisplay);
  },

  /**
   * @param {!chrome.system.display.DisplayUnitInfo} selectedDisplay
   * @param {string} primaryDisplayId
   * @return {boolean}
   * @private
   */
  showMakePrimary_: function(selectedDisplay, primaryDisplayId) {
    return !!selectedDisplay && selectedDisplay.id != primaryDisplayId;
  },

  /**
   * @param {!Array<!chrome.system.display.DisplayUnitInfo>} displays
   * @return {boolean}
   * @private
   */
  showMirror_: function(displays) {
    return this.isMirrored_(displays) || displays.length == 2;
  },

  /**
   * @param {!Array<!chrome.system.display.DisplayUnitInfo>} displays
   * @return {boolean}
   * @private
   */
  isMirrored_: function(displays) {
    return displays.length > 0 && !!displays[0].mirroringSourceId;
  },

  /**
   * @param {!chrome.system.display.DisplayUnitInfo} display
   * @param {!chrome.system.display.DisplayUnitInfo} selectedDisplay
   * @return {boolean}
   * @private
   */
  isSelected_: function(display, selectedDisplay) {
    return display.id == selectedDisplay.id;
  },

  /**
   * @param {!chrome.system.display.DisplayUnitInfo} selectedDisplay
   * @return {boolean}
   * @private
   */
  enableSetResolution_: function(selectedDisplay) {
    return selectedDisplay.modes.length > 1;
  },

  /**
   * @param {!chrome.system.display.DisplayUnitInfo} selectedDisplay
   * @param {number} immediateSelectedModeIndex
   * @return {string}
   * @private
   */
  getResolutionText_: function(selectedDisplay, immediateSelectedModeIndex) {
    if (this.selectedDisplay.modes.length == 0) {
      var widthStr = selectedDisplay.bounds.width.toString();
      var heightStr = selectedDisplay.bounds.height.toString();
      return this.i18n('displayResolutionText', widthStr, heightStr);
    }
    if (isNaN(immediateSelectedModeIndex))
      immediateSelectedModeIndex = this.getSelectedModeIndex_(selectedDisplay);
    var mode = selectedDisplay.modes[immediateSelectedModeIndex];
    var best = selectedDisplay.isInternal ? mode.uiScale == 1.0 : mode.isNative;
    var widthStr = mode.width.toString();
    var heightStr = mode.height.toString();
    if (best)
      return this.i18n('displayResolutionTextBest', widthStr, heightStr);
    else if (mode.isNative)
      return this.i18n('displayResolutionTextNative', widthStr, heightStr);
    return this.i18n('displayResolutionText', widthStr, heightStr);
  },

  /**
   * @param {!{model: !{index: number}, target: !PaperButtonElement}} e
   * @private
   */
  onSelectDisplayTap_: function(e) {
    this.selectedDisplay = this.displays[e.model.index];
    // Force active in case selected display was clicked.
    e.target.active = true;
  },

  /** @private */
  onMakePrimaryTap_: function() {
    if (!this.selectedDisplay)
      return;
    if (this.selectedDisplay.id == this.primaryDisplayId)
      return;
    /** @type {!chrome.system.display.DisplayProperties} */ var properties = {
      isPrimary: true
    };
    settings.display.systemDisplayApi.setDisplayProperties(
        this.selectedDisplay.id, properties,
        this.setPropertiesCallback_.bind(this));
  },

  /**
   * @param {!{target: !PaperSliderElement}} e
   * @private
   */
  onChangeMode_: function(e) {
    var curIndex = this.selectedModeIndex_;
    var newIndex = parseInt(e.target.value, 10);
    if (newIndex == curIndex)
      return;
    assert(newIndex >= 0);
    assert(newIndex < this.selectedDisplay.modes.length);
    /** @type {!chrome.system.display.DisplayProperties} */ var properties = {
      displayMode: this.selectedDisplay.modes[newIndex]
    };
    settings.display.systemDisplayApi.setDisplayProperties(
        this.selectedDisplay.id, properties,
        this.setPropertiesCallback_.bind(this));
  },

  /**
   * @param {!{detail: !{selected: string}}} e
   * @private
   */
  onSetOrientation_: function(e) {
    /** @type {!chrome.system.display.DisplayProperties} */ var properties = {
      rotation: parseInt(e.detail.selected, 10)
    };
    settings.display.systemDisplayApi.setDisplayProperties(
        this.selectedDisplay.id, properties,
        this.setPropertiesCallback_.bind(this));
  },

  /** @private */
  onMirroredTap_: function() {
    var id = '';
    /** @type {!chrome.system.display.DisplayProperties} */ var properties = {};
    if (this.isMirrored_(this.displays)) {
      id = this.primaryDisplayId;
      properties.mirroringSourceId = '';
    } else {
      // Set the mirroringSourceId of the secondary (first non-primary) display.
      for (var display of this.displays) {
        if (display.id != this.primaryDisplayId) {
          id = display.id;
          break;
        }
      }
      properties.mirroringSourceId = this.primaryDisplayId;
    }
    settings.display.systemDisplayApi.setDisplayProperties(
        id, properties, this.setPropertiesCallback_.bind(this));
  },

  /**
   * @param {!Array<!chrome.system.display.DisplayUnitInfo>} displays
   * @private
   */
  updateDisplayInfo_(displays) {
    this.displays = displays;
    var primaryDisplay = undefined;
    var selectedDisplay = undefined;
    for (var display of this.displays) {
      if (display.isPrimary && !primaryDisplay)
        primaryDisplay = display;
      if (this.selectedDisplay && display.id == this.selectedDisplay.id)
        selectedDisplay = display;
    }
    this.primaryDisplayId = (primaryDisplay && primaryDisplay.id) || '';
    this.selectedDisplay = selectedDisplay || primaryDisplay ||
        (this.displays && this.displays[0]);
  },

  /** @private */
  setPropertiesCallback_: function() {
    if (chrome.runtime.lastError) {
      console.error(
          'setDisplayProperties Error: ' + chrome.runtime.lastError.message);
    }
  },
});
