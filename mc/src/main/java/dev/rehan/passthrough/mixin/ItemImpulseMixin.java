package dev.rehan.passthrough.mixin;

import dev.rehan.passthrough.Passthrough;
import net.minecraft.world.entity.Entity;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.world.item.enchantment.effects.ApplyEntityImpulse;
import org.joml.Quaternionf;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Redirect;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

/** Server-side Lunge must use the host view, rather than a delayed movement-packet rotation. */
@Mixin(ApplyEntityImpulse.class)
abstract class ItemImpulseMixin {
    @Inject(method = "apply", at = @At("HEAD"))
    private void doomcraft$currentVelocity(net.minecraft.server.level.ServerLevel level, int enchantmentLevel, net.minecraft.world.item.enchantment.EnchantedItemInUse item, Entity entity, net.minecraft.world.phys.Vec3 position, CallbackInfo ci) {
        // Server travel is suppressed: its old impulse otherwise survives indefinitely.
        if (Passthrough.active && entity instanceof ServerPlayer) entity.setDeltaMovement(Passthrough.hostVelocity);
    }
    @Redirect(method = "apply", at = @At(value = "INVOKE", target = "Lnet/minecraft/world/entity/Entity;getLookQuaternion()Lorg/joml/Quaternionf;"))
    private Quaternionf doomcraft$impulseAim(Entity entity) {
        if (Passthrough.active && entity instanceof ServerPlayer) {
            float[] aim = Passthrough.hostAim;
            return Entity.calculateViewQuaternion(aim[1], aim[0]);
        }
        return entity.getLookQuaternion();
    }
}
