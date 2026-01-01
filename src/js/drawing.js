'use strict';

const manipulator = {
    get: function() {
        console.log("Get function called.")
        return this.$manipulatorTarget.get('value');
    },
    set: function(value) {
        console.log("Set function called.")
        // value = this.roundColorToLayout(value || 0);

        // if (this.get() === value) { return this; }
        this.$manipulatorTarget.set('value', value);
        return this.trigger('change');
    }
};

const template = `
<div class="component component-drawing">
    <label class="tap-highlight">
        <input data-manipulator-target type="hidden"/>
        <span class="label">{{{label}}}</span>
        <span class="value"></span>
    </label>
    {{if description}}
        <div class="description">{{{description}}}</div>
    {{/if}}
    <div class="picker-wrap">
        <div class="picker">
            <div class="pixel-grid-wrap">
                <div class="pixel-grid-container"></div>
            </div>
        </div>
    </div>
</div>
`;

const style = `
.component-drawing label {
    background-color: #000000;
}
`;

const initialize = function(minified, clay) {
    var HTML = minified.HTML;
    let $elem = this.$element;
    let $valueDisplay = $elem.select('.value');
    let $picker = $elem.select('.picker-wrap');
    let disabled = this.$manipulatorTarget.get('disabled');

    $elem.select('label').on('click', function() {
        if (!disabled) {
            $picker.set('show');
        }
    });
    $picker.on('click', function() {
        $picker.set('-show');
    })
};

module.exports = {
    name: 'drawing',
    template: template,
    style: style,
    manipulator: manipulator,
    defaults: {
        label: '',
        description: ''
    },
    initialize: initialize
};
