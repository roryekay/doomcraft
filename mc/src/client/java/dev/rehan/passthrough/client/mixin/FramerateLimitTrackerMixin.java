package dev.rehan.passthrough.client.mixin;

import com.mojang.blaze3d.platform.FramerateLimitTracker;
import dev.rehan.passthrough.Passthrough;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

/** A hidden Minecraft window still supplies the host's live picture. */
@Mixin(FramerateLimitTracker.class)
abstract class FramerateLimitTrackerMixin {
	@Inject(method = "getThrottleReason", at = @At("HEAD"), cancellable = true)
	private void passthrough$keepExportRunning(
		final CallbackInfoReturnable<FramerateLimitTracker.FramerateThrottleReason> cir) {
		if (Passthrough.active) {
			cir.setReturnValue(FramerateLimitTracker.FramerateThrottleReason.NONE);
		}
	}
}
