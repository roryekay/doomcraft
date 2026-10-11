package dev.rehan.passthrough.mixin;

import dev.rehan.passthrough.Passthrough;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.entity.projectile.FireworkRocketEntity;
import net.minecraft.world.phys.Vec3;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.Shadow;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Redirect;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;
import java.util.Locale;

/** Report rocket bursts to Doom; leave vanilla elytra rocket acceleration intact. */
@Mixin(FireworkRocketEntity.class)
abstract class FireworkRocketEntityMixin {
	@Shadow private LivingEntity attachedToEntity;
	@Inject(method = "explode", at = @At("HEAD"))
	private void doomcraft$burst(final ServerLevel level, final CallbackInfo ci) {
		FireworkRocketEntity self = (FireworkRocketEntity)(Object)this;
		if (Passthrough.active && attachedToEntity == null) Passthrough.events.accept(String.format(Locale.ROOT,
			"{\"t\":\"explosion\",\"pos\":[%.3f,%.3f,%.3f],\"r\":4,\"src\":\"firework\",\"id\":%d}",self.getX(),self.getY(),self.getZ(),self.getId()));
	}
}
