<script setup>
import Tick02Icon from '@hugeicons/core-free-icons/Tick02Icon';
import { HugeiconsIcon } from '@hugeicons/vue';
import { ResponsiveDialog } from '@brumaombra/ui-vintage/responsive-dialog';

// Props: the options to pick from ({ value, label, icon?, description? }) and the current one
const props = defineProps({
    visible: { type: Boolean, default: false },
    title: { type: String, required: true },
    description: { type: String, default: '' },
    options: { type: Array, required: true },
    modelValue: { type: [String, Number, Boolean], default: null }
});

// Emits
const emits = defineEmits(['close', 'update:modelValue']);

// Handle close
const handleClose = () => {
    emits('close');
};

// Pick an option (a tap on the current one just closes)
const handleSelect = value => {
    if (value !== props.modelValue) emits('update:modelValue', value);
    handleClose();
};
</script>

<template>
    <ResponsiveDialog :open="props.visible" :title="props.title" :description="props.description" @update:open="value => { if (!value) handleClose(); }">
        <!-- Options, the current one ticked (a sheet on phones, a dialog on larger screens) -->
        <ul class="-mx-2 divide-y divide-border">
            <li v-for="option in props.options" :key="String(option.value)">
                <button type="button" class="flex min-h-12 w-full cursor-pointer items-center gap-3 rounded px-2 py-2 text-left transition-colors hover:bg-accent" :aria-pressed="option.value === props.modelValue" @click="handleSelect(option.value)">
                    <!-- Icon of the option (the slot can draw another one, like a coin logo) -->
                    <slot name="icon" :option="option">
                        <HugeiconsIcon v-if="option.icon" :icon="option.icon" class="size-4 shrink-0 text-muted-foreground" />
                    </slot>
                    <span class="min-w-0 flex-1">
                        <span class="block text-sm font-semibold">{{ option.label }}</span>
                        <span v-if="option.description" class="mt-0.5 block text-xs text-muted-foreground">{{ option.description }}</span>
                    </span>
                    <HugeiconsIcon v-if="option.value === props.modelValue" :icon="Tick02Icon" class="size-4 shrink-0 text-primary" />
                </button>
            </li>
        </ul>
    </ResponsiveDialog>
</template>