package dev.rehan.passthrough.client.mixin;

import dev.rehan.passthrough.Passthrough;
import net.minecraft.client.renderer.ScreenEffectRenderer;
import net.minecraft.client.renderer.SubmitNodeCollector;
import net.minecraft.client.renderer.state.level.PlayerRenderState;
import net.minecraft.client.renderer.state.level.CameraRenderState;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

/** Host wall proxies must not draw Minecraft's opaque in-block screen over Doom. */
@Mixin(ScreenEffectRenderer.class)
abstract class ScreenEffectRendererMixin {
    @Inject(method = "submit", at = @At("HEAD"))
    private void doomcraft$hostWalls(float partialTick, SubmitNodeCollector collector, PlayerRenderState player, CameraRenderState camera, boolean showActivation, CallbackInfo ci) {
        if (Passthrough.active) player.blockOverlay = null;
    }
}
