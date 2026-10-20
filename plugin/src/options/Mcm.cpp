#include "options/Mcm.h"

#include <cstdint>
#include <cstring>

#include "Log.h"
#include "options/Options.h"

namespace {
    // KCSE's plugin interface and messaging, and MCM's settings builder, declared here from their documented ABI
    // (KCSE's KCSEAPI.h, MCM's MCM_API.h) in their declaration order, so MSVC lays the vtables out the same way.
    // Members the plugin never calls still hold their place: removing one would shift the slots and offsets after it.
    struct Message {
        const char *sender;
        std::uint32_t type;
        std::uint32_t dataLen;
        void *data;
    };
    using Listener = void (*)(Message *);

    class KcseInterface {
      public:
        virtual std::uint32_t GetKCSEVersion() const = 0;
        virtual std::uint32_t GetGameVersion() const = 0;
        virtual std::uint32_t GetReleaseIndex() const = 0;
        virtual std::uint32_t GetPluginHandle() const = 0;
        virtual const void *GetPluginInfo(const char *name) const = 0;
        virtual void *QueryInterface(std::uint32_t id) const = 0;

      protected:
        ~KcseInterface() = default; // owned by KCSE, never deleted here
    };
    constexpr std::uint32_t kMessagingInterface = 0;

    class Messaging {
      public:
        virtual std::uint32_t GetInterfaceVersion() const = 0;
        virtual bool RegisterListener(Listener handler) = 0;
        virtual bool RegisterListener(const char *sender, Listener handler) = 0;

      protected:
        ~Messaging() = default; // owned by KCSE, never deleted here
    };

    // Sent by "MCM": each time its menu is built (data: a SettingsBuilder valid during the call), and for each change.
    constexpr std::uint32_t kBuildSettings = 0x4D434D02;
    constexpr std::uint32_t kValueChanged = 0x4D434D01;

    struct ValueChanged {
        const char *modId;
        const char *settingId;
        double value;
    };

    class SettingsBuilder {
      public:
        virtual void AddMod(const char *modId, const char *displayName) = 0;
        virtual void AddCategory(const char *modId, const char *label, const char *tooltip) = 0;
        virtual void AddToggle(const char *modId, const char *id, const char *label, const char *tooltip,
                               bool defaultOn) = 0;
        virtual void AddDropdown(const char *modId, const char *id, const char *label, const char *tooltip,
                                 const char *options, int defaultIndex) = 0;
        virtual void AddSlider(const char *modId, const char *id, const char *label, const char *tooltip, double min,
                               double max, double step, double defaultValue, const char *suffix) = 0;
        virtual void SetValue(const char *modId, const char *id, double value) = 0;

      protected:
        ~SettingsBuilder() = default; // owned by MCM, never deleted here
    };

    constexpr char kModId[] = "treasurefinder";

    // Declares every setting with its default, then shows its current value (so the menu's reset restores the
    // default, not whatever is in effect).
    void Build(SettingsBuilder *builder) {
        static constexpr Options::Values defaults;
        const Options::Values &values = Options::Get();
        builder->AddMod(kModId, "Treasure Finder");
        const char *section = nullptr;
        for (const Options::Setting &setting : Options::Settings()) {
            if (!section || std::strcmp(section, setting.section) != 0) {
                section = setting.section;
                builder->AddCategory(kModId, section, "");
            }
            if (setting.toggle) {
                builder->AddToggle(kModId, setting.id, setting.label, Options::Tooltip(setting),
                                   defaults.*setting.toggle);
                builder->SetValue(kModId, setting.id, values.*setting.toggle ? 1.0 : 0.0);
            } else {
                builder->AddSlider(kModId, setting.id, setting.label, Options::Tooltip(setting), setting.min,
                                   setting.max, setting.step, defaults.*setting.slider, setting.suffix);
                builder->SetValue(kModId, setting.id, values.*setting.slider);
            }
        }
    }

    void OnMessage(Message *message) {
        if (message->type == kBuildSettings && message->data) {
            Build(static_cast<SettingsBuilder *>(message->data));
        } else if (message->type == kValueChanged && message->data) {
            const auto *change = static_cast<const ValueChanged *>(message->data);
            if (std::strcmp(change->modId, kModId) == 0 && !Options::Set(change->settingId, change->value))
                Log::Write("mcm: unknown setting '{}'", change->settingId);
        }
    }
} // namespace

void Mcm::Install(const void *kcse) {
    const auto *root = static_cast<const KcseInterface *>(kcse);
    auto *messaging = root ? static_cast<Messaging *>(root->QueryInterface(kMessagingInterface)) : nullptr;
    if (!messaging || !messaging->RegisterListener("MCM", &OnMessage))
        Log::Write("mcm: KCSE messaging is not available; settings are only in TreasureFinder.ini");
}
