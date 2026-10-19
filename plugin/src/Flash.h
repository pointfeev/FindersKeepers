#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include "Engine.h"

// The game's Scaleform wrappers (CFlashPlayer, CFlashVariableObject), declared from WHGame.dll 1.5.6.
namespace Flash {
    class Player;

    // Interface slots, checked against the vtables in Ghidra.
    namespace Slot {
        // CFlashVariableObject, vtable 0x183A55BA0.
        constexpr std::size_t kRelease = 0;
        constexpr std::size_t kToValue = 5;        // returns the value through a hidden pointer
        constexpr std::size_t kSetMemberVar = 7;   // (name, const Var *)
        constexpr std::size_t kSetMemberValue = 8; // (name, const Value &)
        constexpr std::size_t kGetMemberVar = 9;   // (name, Var *&)
        constexpr std::size_t kInvokeVars = 13;    // (method, const Var *const *, count, Value *result)
        constexpr std::size_t kInvokeValues = 14;  // (method, const Value *, count, Value *result)
        constexpr std::size_t kSetVisible = 33;
        constexpr std::size_t kCreateEmptyMovieClip = 40; // (Var *&, name, depth)
        // CFlashPlayer, vtable 0x183DC07A8.
        constexpr std::size_t kGetVariableVar = 40; // (path, Var *&)
        constexpr std::size_t kIsAvailable = 42;
        constexpr std::size_t kCreateFunction = 51; // (Var *&, Function *, userData)
        // FunctionHandlerAdaptor::ReturnValue, vtable 0x1846AD358: [0] takes a Var, [1] a Value.
        constexpr std::size_t kResultSetValue = 1; // (const Value &, bool createManagedValue)
    } // namespace Slot

    // SFlashVarValue: the type, then 8 bytes of data (16 bytes).
    struct Value {
        enum Type : std::int32_t {
            Undefined,
            Null,
            Bool,
            Int,
            UInt,
            Double,
            Float,
            String,
            WideString,
            Object,
        };
        Type type = Undefined;
        union {
            bool b;
            std::int32_t i;
            std::uint32_t u;
            double d;
            float f;
            const char *string;
            const wchar_t *wideString;
        };

        Value() : d(0.0) {}
        Value(int v) : type(Int), i(v) {}
        Value(float v) : type(Float), f(v) {}
        Value(const char *v) : type(String), string(v) {}
    };
    static_assert(sizeof(Value) == 16, "SFlashVarValue is 16 bytes");

    // A reference to an ActionScript value; released, not deleted.
    class Var {
      public:
        void Release() { Engine::VCall<void>(this, Slot::kRelease); }
        Value ToValue() const {
            Value out;
            Engine::VCall<Value *>(this, Slot::kToValue, &out);
            return out;
        }
        bool GetMember(const char *name, Var *&out) const {
            return Engine::VCall<bool>(this, Slot::kGetMemberVar, name, &out);
        }
        bool SetMember(const char *name, const Value &value) {
            return Engine::VCall<bool>(this, Slot::kSetMemberValue, name, &value);
        }
        bool SetMember(const char *name, const Var *value) {
            return Engine::VCall<bool>(this, Slot::kSetMemberVar, name, value);
        }
        bool Invoke(const char *method, std::span<const Value> args, Value *result = nullptr) {
            return Engine::VCall<bool>(this, Slot::kInvokeValues, method, args.data(),
                                       static_cast<unsigned>(args.size()), result);
        }
        bool Invoke(const char *method, std::span<const Var *const> args, Value *result = nullptr) {
            return Engine::VCall<bool>(this, Slot::kInvokeVars, method, args.data(), static_cast<unsigned>(args.size()),
                                       result);
        }
        bool SetVisible(bool visible) { return Engine::VCall<bool>(this, Slot::kSetVisible, visible); }
        bool CreateEmptyMovieClip(Var *&out, const char *name, int depth = -1) {
            return Engine::VCall<bool>(this, Slot::kCreateEmptyMovieClip, &out, name, depth);
        }
    };

    // Owns one Var reference.
    class VarPtr {
      public:
        VarPtr() = default;
        VarPtr(const VarPtr &) = delete;
        VarPtr &operator=(const VarPtr &) = delete;
        ~VarPtr() {
            if (var_)
                var_->Release();
        }
        // For out-parameters; releases what it held.
        Var *&put() {
            if (var_)
                var_->Release();
            var_ = nullptr;
            return var_;
        }
        Var *get() const { return var_; }
        Var *operator->() const { return var_; }
        explicit operator bool() const { return var_ != nullptr; }

      private:
        Var *var_ = nullptr;
    };

    // A native ActionScript function (IActionScriptFunction): the game calls slot 0, Call, through its
    // FunctionHandlerAdaptor (0x182449954).
    class Function {
      public:
        struct Params {
            Player *player;
            void *userData;
            const Var *self;
            const Var *const *args;
            unsigned count;

            std::span<const Var *const> Args() const { return {args, count}; }
        };
        // The adaptor's ReturnValue.
        class Result {
          public:
            void Set(const Value &value) { Engine::VCall<void>(this, Slot::kResultSetValue, &value, true); }
        };

        virtual void Call(const Params &params, Result *result) = 0;

      protected:
        virtual ~Function() = default;
    };

    class Player {
      public:
        bool IsAvailable(const char *path) const { return Engine::VCall<bool>(this, Slot::kIsAvailable, path); }
        bool GetVariable(const char *path, Var *&out) const {
            return Engine::VCall<bool>(this, Slot::kGetVariableVar, path, &out);
        }
        bool CreateFunction(Var *&out, Function *function, void *userData = nullptr) {
            return Engine::VCall<bool>(this, Slot::kCreateFunction, &out, function, userData);
        }
    };
} // namespace Flash
