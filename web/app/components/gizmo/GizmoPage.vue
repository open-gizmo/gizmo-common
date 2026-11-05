<script setup>
// Props: the title of the page (its h1), and whether to hide it on screen when a card at the top already names the page (screen readers still read it)
const props = defineProps({
    title: { type: String, required: true },
    hideTitle: { type: Boolean, default: false }
});
</script>

<template>
    <div class="mx-auto w-full max-w-2xl space-y-6">
        <!-- One centered column per page, like the dashboard pages of KrowdCall: the title, then the groups and the main visual of the page -->
        <!-- Title -->
        <h1 :class="props.hideTitle ? 'sr-only' : 'flex min-h-10 items-center px-1 text-xl font-semibold tracking-tight text-foreground md:text-2xl'">{{ props.title }}</h1>

        <!-- Content of the page -->
        <slot />
    </div>
</template>
