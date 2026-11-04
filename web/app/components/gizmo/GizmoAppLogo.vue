<script setup>
const appConfig = useAppConfig();
</script>

<template>
    <div class="flex items-center gap-2 sm:gap-3">
        <!-- Brand mark (from the app.config.ts of the app) -->
        <img :src="appConfig.gizmo.logo" alt="" aria-hidden="true" class="size-8 sm:size-9">

        <!-- Brand name -->
        <span class="text-base font-bold text-foreground sm:text-lg">{{ appConfig.gizmo.name }}</span>
    </div>
</template>