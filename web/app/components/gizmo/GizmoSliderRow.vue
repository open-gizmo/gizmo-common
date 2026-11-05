<script setup>
import { computed } from 'vue';
import { HugeiconsIcon } from '@hugeicons/vue';
import { Slider } from '@brumaombra/ui-vintage/slider';

// Props: a slider for a ListGroup. The label and the current value are on the first line, the slider under them at full width, so dragging it while watching the device stays one gesture
const props = defineProps({
    label: { type: String, required: true },
    modelValue: { type: Number, required: true },
    icon: { type: [Array, Object], default: null },
    min: { type: Number, default: 0 },
    max: { type: Number, default: 100 },
    step: { type: Number, default: 1 },
    valueText: { type: String, default: '' } // Text of the current value (for example "60%"), the number itself when empty
});

// Emits
const emits = defineEmits(['update:modelValue']);

// Current value as the slider wants it
const sliderValue = computed(() => [props.modelValue]);

// Current value shown on the first line
const resolvedValueText = computed(() => props.valueText || String(props.modelValue));

// Pass the first value of the slider to the parent
const handleUpdateModelValue = value => {
    emits('update:modelValue', value?.[0] ?? props.min);
};
</script>

<template>
    <li data-slot="list-row" class="px-4 py-2 md:px-5">
        <!-- Row of a ListGroup (same padding and text as a ListRow) -->
        <!-- Label and current value -->
        <div class="flex items-center gap-2 pt-1 md:gap-3">
            <HugeiconsIcon v-if="props.icon" :icon="props.icon" class="size-4 shrink-0 text-muted-foreground" />
            <span class="min-w-0 flex-1 text-[0.8125rem] font-semibold md:text-sm">{{ props.label }}</span>
            <span class="font-mono text-[0.6875rem] text-muted-foreground md:text-xs">{{ resolvedValueText }}</span>
        </div>

        <!-- Slider, full width -->
        <Slider :model-value="sliderValue" :min="props.min" :max="props.max" :step="props.step" :thumb-label="props.label" @update:model-value="handleUpdateModelValue" />
    </li>
</template>
