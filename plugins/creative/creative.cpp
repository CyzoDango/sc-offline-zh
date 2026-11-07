// creative: sc-offline's cheats (god mode, noclip and its speed, infinite ammo, infinite ship ammo) as
// an optional plugin on sco-core's game.creative service. A plain SDK consumer: it includes only the
// SDK zip's headers (the sdk-headers gate covers this folder with no allow-list entry) and does
// nothing the SDK does not offer any other plugin.
//
// The commands are the old cheats' controls: creative.god, creative.noclip, creative.noclip_speed,
// creative.ammo, creative.ship_ammo and creative.status. A toggle this plugin switches on goes off
// when the plugin unloads (the game pack's release hook), so unloading it is the off switch for
// everything it did. The folder ships with a `disabled` file: turn the plugin on from the launcher's
// Plugins page.
#include "scosdk/scosdk.hpp"
#include "scosdk/game/creative.hpp"

#include <cstdint>
#include <string>

namespace {

class Creative : public sco::sdk::Plugin {
public:
    sco_result OnLoad() override {
        using namespace sco::sdk;
        const sco_result open = cr_.Open(*this);
        if (open != SCO_OK) {
            Error("game.creative isn't available (result %d)", static_cast<int>(open));
            return open;
        }
        sco_result r = Toggle("creative.god", "无敌模式", "人物不受伤害", "game.creative.god_mode",
                              &sco::sdk::game::Creative::SetGodMode);
        if (r == SCO_OK)
            r = Toggle("creative.noclip", "穿墙", "自由穿过物体（速度：creative.noclip_speed）", "game.creative.fly",
                       &sco::sdk::game::Creative::SetFlyMode);
        if (r == SCO_OK)
            r = Toggle("creative.ammo", "无限弹药", "随身武器的弹药不会耗尽", "game.creative.ammo",
                       &sco::sdk::game::Creative::SetInfiniteAmmo);
        if (r == SCO_OK)
            r = Toggle("creative.ship_ammo", "载具无限弹药", "当前乘坐载具的武器弹药不会耗尽",
                       "game.creative.ship_ammo", &sco::sdk::game::Creative::SetInfiniteShipAmmo);
        if (r == SCO_OK)
            r = CommandBuilder(*this, "creative.noclip_speed")
                    .Title("穿墙速度")
                    .Help("自由飞行速度（1 至 10000 米/秒）；卸载插件后恢复游戏原来的速度")
                    .Capability("game.creative.fly")
                    .Arg<double>("speed", "米/秒")
                    .Handle([this](const Args& args, Reply& reply) {
                        const double speed = args.Float(0);
                        const sco_result s = cr_.SetFlySpeed(static_cast<float>(speed));
                        if (s != SCO_OK) return Failed(reply, "穿墙速度", s);
                        reply.Printf("穿墙速度 %.0f", speed);
                        return SCO_OK;
                    })
                    .Register();
        if (r == SCO_OK)
            r = CommandBuilder(*this, "creative.status")
                    .Title("创意模式状态")
                    .Help("查看各个功能开关")
                    .Handle([this](const Args&, Reply& reply) {
                        uint32_t flags = 0;
                        const sco_result s = cr_.State(flags);
                        if (s != SCO_OK) return Failed(reply, "创意模式状态", s);
                        reply.Printf("无敌 %s，穿墙 %s，弹药 %s，载具弹药 %s", On(flags, SC_CREATIVE_GOD_MODE),
                                     On(flags, SC_CREATIVE_FLY), On(flags, SC_CREATIVE_INFINITE_AMMO),
                                     On(flags, SC_CREATIVE_INFINITE_SHIP_AMMO));
                        return SCO_OK;
                    })
                    .Register();
        return r;
    }

private:
    using SetFn = sco_result (sco::sdk::game::Creative::*)(bool) const noexcept;

    static const char* On(uint32_t flags, uint32_t bit) { return (flags & bit) ? "开启" : "关闭"; }

    // The reply says why: the game pack's own reason, when it left one.
    sco_result Failed(sco::sdk::Reply& reply, const char* what, sco_result r) const {
        char why[256] = {};
        cr_.LastError(why);
        reply.Printf("%s 失败：%s", what, why[0] ? why : "请查看 mod.log");
        return r;
    }

    sco_result Toggle(const char* name, const char* title, const char* help, const char* capability, SetFn set) {
        using namespace sco::sdk;
        const std::string label = title;
        return CommandBuilder(*this, name)
            .Title(title)
            .Help(help)
            .Capability(capability)
            .Arg<bool>("on", "true = 开启，false = 关闭")
            .Handle([this, set, label](const Args& args, Reply& reply) {
                const bool on = args.Bool(0);
                const sco_result s = (cr_.*set)(on);
                if (s != SCO_OK) return Failed(reply, label.c_str(), s);
                reply.Printf("%s %s", label.c_str(), on ? "已开启" : "已关闭");
                return SCO_OK;
            })
            .Register();
    }

    sco::sdk::game::Creative cr_;
};

}  // namespace

// id must equal plugin.ini's id; it is also the command prefix.
SCO_PLUGIN(Creative, "creative", "1.0.0", "sc-offline");
