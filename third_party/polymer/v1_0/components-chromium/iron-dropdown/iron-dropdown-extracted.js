(function() {
      'use strict';

      Polymer({
        is: 'iron-dropdown',

        behaviors: [
          Polymer.IronControlState,
          Polymer.IronA11yKeysBehavior,
          Polymer.IronOverlayBehavior,
          Polymer.NeonAnimationRunnerBehavior
        ],

        properties: {
          /**
           * The orientation against which to align the dropdown content
           * horizontally relative to the dropdown trigger.
           */
          horizontalAlign: {
            type: String,
            value: 'left',
            reflectToAttribute: true
          },

          /**
           * The orientation against which to align the dropdown content
           * vertically relative to the dropdown trigger.
           */
          verticalAlign: {
            type: String,
            value: 'top',
            reflectToAttribute: true
          },

          /**
           * A pixel value that will be added to the position calculated for the
           * given `horizontalAlign`, in the direction of alignment. You can think
           * of it as increasing or decreasing the distance to the side of the
           * screen given by `horizontalAlign`.
           *
           * If `horizontalAlign` is "left", this offset will increase or decrease
           * the distance to the left side of the screen: a negative offset will
           * move the dropdown to the left; a positive one, to the right.
           *
           * Conversely if `horizontalAlign` is "right", this offset will increase
           * or decrease the distance to the right side of the screen: a negative
           * offset will move the dropdown to the right; a positive one, to the left.
           */
          horizontalOffset: {
            type: Number,
            value: 0,
            notify: true
          },

          /**
           * A pixel value that will be added to the position calculated for the
           * given `verticalAlign`, in the direction of alignment. You can think
           * of it as increasing or decreasing the distance to the side of the
           * screen given by `verticalAlign`.
           *
           * If `verticalAlign` is "top", this offset will increase or decrease
           * the distance to the top side of the screen: a negative offset will
           * move the dropdown upwards; a positive one, downwards.
           *
           * Conversely if `verticalAlign` is "bottom", this offset will increase
           * or decrease the distance to the bottom side of the screen: a negative
           * offset will move the dropdown downwards; a positive one, upwards.
           */
          verticalOffset: {
            type: Number,
            value: 0,
            notify: true
          },

          /**
           * The element that should be used to position the dropdown when
           * it is opened.
           */
          positionTarget: {
            type: Object
          },

          /**
           * An animation config. If provided, this will be used to animate the
           * opening of the dropdown.
           */
          openAnimationConfig: {
            type: Object
          },

          /**
           * An animation config. If provided, this will be used to animate the
           * closing of the dropdown.
           */
          closeAnimationConfig: {
            type: Object
          },

          /**
           * If provided, this will be the element that will be focused when
           * the dropdown opens.
           */
          focusTarget: {
            type: Object
          },

          /**
           * Set to true to disable animations when opening and closing the
           * dropdown.
           */
          noAnimations: {
            type: Boolean,
            value: false
          },

          /**
           * By default, the dropdown will constrain scrolling on the page
           * to itself when opened.
           * Set to true in order to prevent scroll from being constrained
           * to the dropdown when it opens.
           */
          allowOutsideScroll: {
            type: Boolean,
            value: false
          }
        },

        listeners: {
          'neon-animation-finish': '_onNeonAnimationFinish'
        },

        observers: [
          '_updateOverlayPosition(positionTarget, verticalAlign, horizontalAlign, verticalOffset, horizontalOffset)'
        ],

        attached: function() {
          // Memoize this to avoid expensive calculations & relayouts.
          this._isRTL = window.getComputedStyle(this).direction == 'rtl';
          this.positionTarget = this.positionTarget || this._defaultPositionTarget;
        },

        /**
         * The element that is contained by the dropdown, if any.
         */
        get containedElement() {
          return Polymer.dom(this.$.content).getDistributedNodes()[0];
        },

        /**
         * The element that should be focused when the dropdown opens.
         * @deprecated
         */
        get _focusTarget() {
          return this.focusTarget || this.containedElement;
        },

        /**
         * The element that should be used to position the dropdown when
         * it opens, if no position target is configured.
         */
        get _defaultPositionTarget() {
          var parent = Polymer.dom(this).parentNode;

          if (parent.nodeType === Node.DOCUMENT_FRAGMENT_NODE) {
            parent = parent.host;
          }

          return parent;
        },

        /**
         * The horizontal align value, accounting for the RTL/LTR text direction.
         */
        get _localeHorizontalAlign() {
          // In RTL, "left" becomes "right".
          if (this._isRTL) {
            return this.horizontalAlign === 'right' ? 'left' : 'right';
          } else {
            return this.horizontalAlign;
          }
        },

        /**
         * The horizontal offset value used to position the dropdown.
         * @param {ClientRect} dropdownRect
         * @param {ClientRect} positionRect
         * @param {boolean=} fromRight
         * @return {number} pixels
         * @private
         */
        _horizontalAlignTargetValue: function(dropdownRect, positionRect, fromRight) {
          var target;
          if (fromRight) {
            target = document.documentElement.clientWidth - positionRect.right - (this._fitWidth - dropdownRect.right);
          } else {
            target = positionRect.left - dropdownRect.left;
          }
          target += this.horizontalOffset;

          return Math.max(target, 0);
        },

        /**
         * The vertical offset value used to position the dropdown.
         * @param {ClientRect} dropdownRect
         * @param {ClientRect} positionRect
         * @param {boolean=} fromBottom
         * @return {number} pixels
         * @private
         */
        _verticalAlignTargetValue: function(dropdownRect, positionRect, fromBottom) {
          var target;
          if (fromBottom) {
            target = document.documentElement.clientHeight - positionRect.bottom - (this._fitHeight - dropdownRect.bottom);
          } else {
            target = positionRect.top - dropdownRect.top;
          }
          target += this.verticalOffset;

          return Math.max(target, 0);
        },

        /**
         * Called when the value of `opened` changes.
         * Overridden from `IronOverlayBehavior`
         */
        _openedChanged: function() {
          if (this.opened && this.disabled) {
            this.cancel();
          } else {
            this.cancelAnimation();
            this.sizingTarget = this.containedElement || this.sizingTarget;
            this._updateAnimationConfig();
            if (this.opened && !this.allowOutsideScroll) {
              Polymer.IronDropdownScrollManager.pushScrollLock(this);
            } else {
              Polymer.IronDropdownScrollManager.removeScrollLock(this);
            }
            Polymer.IronOverlayBehaviorImpl._openedChanged.apply(this, arguments);
          }
        },

        /**
         * Overridden from `IronOverlayBehavior`.
         */
        _renderOpened: function() {
          if (!this.noAnimations && this.animationConfig && this.animationConfig.open) {
            if (this.withBackdrop) {
              this.backdropElement.open();
            }
            this.$.contentWrapper.classList.add('animating');
            this.playAnimation('open');
          } else {
            Polymer.IronOverlayBehaviorImpl._renderOpened.apply(this, arguments);
          }
        },

        /**
         * Overridden from `IronOverlayBehavior`.
         */
        _renderClosed: function() {
          if (!this.noAnimations && this.animationConfig && this.animationConfig.close) {
            if (this.withBackdrop) {
              this.backdropElement.close();
            }
            this.$.contentWrapper.classList.add('animating');
            this.playAnimation('close');
          } else {
            Polymer.IronOverlayBehaviorImpl._renderClosed.apply(this, arguments);
          }
        },

        /**
         * Called when animation finishes on the dropdown (when opening or
         * closing). Responsible for "completing" the process of opening or
         * closing the dropdown by positioning it or setting its display to
         * none.
         */
        _onNeonAnimationFinish: function() {
          this.$.contentWrapper.classList.remove('animating');
          if (this.opened) {
            Polymer.IronOverlayBehaviorImpl._finishRenderOpened.apply(this);
          } else {
            Polymer.IronOverlayBehaviorImpl._finishRenderClosed.apply(this);
          }
        },

        /**
         * Constructs the final animation config from different properties used
         * to configure specific parts of the opening and closing animations.
         */
        _updateAnimationConfig: function() {
          var animationConfig = {};
          var animations = [];

          if (this.openAnimationConfig) {
            // NOTE(cdata): When making `display:none` elements visible in Safari,
            // the element will paint once in a fully visible state, causing the
            // dropdown to flash before it fades in. We prepend an
            // `opaque-animation` to fix this problem:
            animationConfig.open = [{
              name: 'opaque-animation',
            }].concat(this.openAnimationConfig);
            animations = animations.concat(animationConfig.open);
          }

          if (this.closeAnimationConfig) {
            animationConfig.close = this.closeAnimationConfig;
            animations = animations.concat(animationConfig.close);
          }

          animations.forEach(function(animation) {
            animation.node = this.containedElement;
          }, this);

          this.animationConfig = animationConfig;
        },

        /**
         * Updates the overlay position based on configured horizontal
         * and vertical alignment.
         */
        _updateOverlayPosition: function() {
          if (this.isAttached) {
            // This triggers iron-resize, and iron-overlay-behavior will call refit if needed.
            this.notifyResize();
          }
        },

        /**
         * Useful to call this after the element, the window, or the `fitInfo`
         * element has been resized. Will maintain the scroll position.
         */
        refit: function () {
          if (!this.opened) {
            return
          }
          var containedElement = this.containedElement;
          var scrollTop;
          var scrollLeft;

          if (containedElement) {
            scrollTop = containedElement.scrollTop;
            scrollLeft = containedElement.scrollLeft;
          }
          Polymer.IronFitBehavior.refit.apply(this, arguments);

          if (containedElement) {
            containedElement.scrollTop = scrollTop;
            containedElement.scrollLeft = scrollLeft;
          }
        },

        /**
         * Resets the target element's position and size constraints, and clear
         * the memoized data.
         */
        resetFit: function() {
          Polymer.IronFitBehavior.resetFit.apply(this, arguments);

          var hAlign = this._localeHorizontalAlign;
          var vAlign = this.verticalAlign;
          // Set to 0, 0 in order to discover any offset caused by parent stacking contexts.
          this.style[hAlign] = this.style[vAlign] = '0px';

          var dropdownRect = this.getBoundingClientRect();
          var positionRect = this.positionTarget.getBoundingClientRect();
          var horizontalValue = this._horizontalAlignTargetValue(dropdownRect, positionRect, hAlign === 'right');
          var verticalValue = this._verticalAlignTargetValue(dropdownRect, positionRect, vAlign === 'bottom');

          this.style[hAlign] = horizontalValue + 'px';
          this.style[vAlign] = verticalValue + 'px';
        },

        /**
         * Overridden from `IronFitBehavior`.
         * Ensure positionedBy has correct values for horizontally & vertically.
         */
        _discoverInfo: function() {
          Polymer.IronFitBehavior._discoverInfo.apply(this, arguments);
          // Note(valdrin): in Firefox, an element with style `position: fixed; bottom: 90vh; height: 20vh`
          // would have `getComputedStyle(element).top < 0` (instead of being `auto`) http://jsbin.com/cofired/3/edit?html,output
          // This would cause IronFitBehavior's `constrain` to wrongly calculate sizes
          // (it would use `top` instead of `bottom`), so we ensure we give the correct values.
          this._fitInfo.positionedBy.horizontally = this._localeHorizontalAlign;
          this._fitInfo.positionedBy.vertically = this.verticalAlign;
        },

        /**
         * Apply focus to focusTarget or containedElement
         */
        _applyFocus: function () {
          var focusTarget = this.focusTarget || this.containedElement;
          if (focusTarget && this.opened && !this.noAutoFocus) {
            focusTarget.focus();
          } else {
            Polymer.IronOverlayBehaviorImpl._applyFocus.apply(this, arguments);
          }
        }
      });
    })();