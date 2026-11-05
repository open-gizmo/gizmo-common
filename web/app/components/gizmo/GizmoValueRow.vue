<script setup>
import { ListRow } from '@brumaombra/ui-vintage/list';

// A ListRow whose value wraps instead of being cut with "..." (host names, network names and other long values). It takes the props of ListRow (label, icon, to, action, danger...) and passes them on. The value goes in the trailing slot, up to 60% of the row, wrapped at the spaces first, and at any character when a name has none
defineOptions({ inheritAttrs: false });

// Props: the value on the right (the rest goes to the ListRow)
const props = defineProps({
    value: { type: String, default: '' }
});
</script>

<template>
    <ListRow v-bind="$attrs">
        <template #trailing>
            <span v-if="props.value" class="min-w-0 max-w-[60%] wrap-anywhere text-right font-mono text-[0.6875rem] text-muted-foreground md:text-xs">{{ props.value }}</span>
        </template>
    </ListRow>
</template>
