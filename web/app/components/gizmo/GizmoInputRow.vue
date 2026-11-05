<script setup>
import { HugeiconsIcon } from '@hugeicons/vue';
import { Input } from '@brumaombra/ui-vintage/input';

// Props: a text field for a ListGroup. The icon and the label are on the first line, the field under them at full width (the help text goes in the hint of the group)
const props = defineProps({
    id: { type: String, required: true },
    label: { type: String, required: true },
    modelValue: { type: String, default: '' },
    icon: { type: [Array, Object], default: null },
    type: { type: String, default: 'text' },
    placeholder: { type: String, default: '' },
    autocomplete: { type: String, default: 'off' },
    maxlength: { type: Number, default: undefined },
    disabled: { type: Boolean, default: false }
});

// Emits
const emits = defineEmits(['update:modelValue']);

// Pass what the user typed to the parent
const handleUpdateModelValue = value => {
    emits('update:modelValue', String(value));
};
</script>

<template>
    <li data-slot="list-row" class="space-y-2 px-4 py-3 md:px-5">
        <!-- Row of a ListGroup (same padding and text as a ListRow) -->
        <!-- Icon and label (a tap on the label focuses the field) -->
        <div class="flex items-center gap-2 md:gap-3">
            <HugeiconsIcon v-if="props.icon" :icon="props.icon" class="size-4 shrink-0 text-muted-foreground" />
            <label :for="props.id" class="min-w-0 flex-1 text-[0.8125rem] font-semibold md:text-sm">{{ props.label }}</label>
        </div>

        <!-- Field, full width -->
        <Input :id="props.id" :model-value="props.modelValue" :type="props.type" :placeholder="props.placeholder" :autocomplete="props.autocomplete" :maxlength="props.maxlength" :disabled="props.disabled" @update:model-value="handleUpdateModelValue" />
    </li>
</template>
