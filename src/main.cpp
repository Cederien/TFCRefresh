// TFCRefresh - while in free camera (tfc), the player's 3D isn't rebuilt after
// equipment changes until free camera is left. This plugin leaves and re-enters
// free camera for a moment and restores the camera position/rotation, so the
// change becomes visible without losing the shot.
//
// Triggers: player equip/unequip events while in free camera (debounced), and
// an optional hotkey. Settings: Data/SKSE/Plugins/TFCRefresh.ini

#include <Windows.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

using namespace std::chrono_literals;

namespace
{
	struct Settings
	{
		bool          autoOnEquip = true;
		std::uint32_t hotkey = 0;        // DirectInput scancode, 0 = off
		std::uint32_t debounceMs = 250;  // wait for a burst of equip events to settle
		std::uint32_t gapMs = 50;        // time spent outside free camera

		void Load()
		{
			const auto path = L"Data\\SKSE\\Plugins\\TFCRefresh.ini";
			autoOnEquip = GetPrivateProfileIntW(L"General", L"bAutoRefreshOnEquip", 1, path) != 0;
			hotkey = GetPrivateProfileIntW(L"General", L"iHotkey", 0, path);
			debounceMs = GetPrivateProfileIntW(L"General", L"iDebounceMs", 250, path);
			gapMs = GetPrivateProfileIntW(L"General", L"iGapMs", 50, path);
			SKSE::log::info("settings: autoOnEquip={} hotkey={} debounceMs={} gapMs={}", autoOnEquip, hotkey, debounceMs, gapMs);
		}
	} g_settings;

	struct SavedCam
	{
		RE::NiPoint3          translation;
		RE::BSTPoint2<float>  rotation;
		bool                  lockToZPlane{ false };
		bool                  useRunSpeed{ false };
		bool                  freezeTime{ false };
	};

	RE::FreeCameraState* GetFreeState()
	{
		auto* camera = RE::PlayerCamera::GetSingleton();
		if (!camera || !camera->IsInFreeCameraMode()) {
			return nullptr;
		}
		auto* state = camera->currentState.get();
		if (!state || state->id != RE::CameraState::kFree) {
			return nullptr;
		}
		return static_cast<RE::FreeCameraState*>(state);
	}

	void RunOnMain(std::function<void()> a_fn)
	{
		std::mutex              m;
		std::condition_variable cv;
		bool                    done = false;
		SKSE::GetTaskInterface()->AddTask([&] {
			a_fn();
			std::lock_guard lk(m);
			done = true;
			cv.notify_one();
		});
		std::unique_lock lk(m);
		cv.wait_for(lk, 5s, [&] { return done; });
	}

	// one refresh cycle, run from the worker thread
	void DoRefresh()
	{
		SavedCam saved;
		bool     left = false;

		RunOnMain([&] {
			auto* ui = RE::UI::GetSingleton();
			if (ui && ui->GameIsPaused()) {
				return;  // menu open - try again later
			}
			auto* fs = GetFreeState();
			if (!fs) {
				return;
			}
			saved.translation = fs->translation;
			saved.rotation = fs->rotation;
			saved.lockToZPlane = fs->lockToZPlane;
			saved.useRunSpeed = fs->useRunSpeed;
			saved.freezeTime = RE::Main::GetSingleton()->freezeTime;
			RE::PlayerCamera::GetSingleton()->ToggleFreeCameraMode(false);
			left = true;
		});
		if (!left) {
			return;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(g_settings.gapMs));

		auto restore = [&] {
			if (auto* fs = GetFreeState()) {
				fs->translation = saved.translation;
				fs->rotation = saved.rotation;
				fs->lockToZPlane = saved.lockToZPlane;
				fs->useRunSpeed = saved.useRunSpeed;
			}
		};
		RunOnMain([&] {
			auto* camera = RE::PlayerCamera::GetSingleton();
			if (!camera->IsInFreeCameraMode()) {
				camera->ToggleFreeCameraMode(saved.freezeTime);
			}
			restore();
		});
		// Begin() may settle on the next camera update; pin the position once more
		std::this_thread::sleep_for(20ms);
		RunOnMain(restore);
		SKSE::log::debug("refreshed player 3D in free camera");
	}

	class Worker
	{
	public:
		void Request(bool a_immediate)
		{
			{
				std::lock_guard lk(_m);
				_pending = true;
				_due = std::chrono::steady_clock::now() +
				       (a_immediate ? 0ms : std::chrono::milliseconds(g_settings.debounceMs));
			}
			_cv.notify_one();
		}

		void Start()
		{
			std::thread([this] { Run(); }).detach();
		}

		std::atomic_bool busy{ false };

	private:
		void Run()
		{
			std::unique_lock lk(_m);
			for (;;) {
				_cv.wait(lk, [&] { return _pending; });
				// debounce: keep waiting while new requests push the deadline out
				while (std::chrono::steady_clock::now() < _due) {
					_cv.wait_until(lk, _due);
				}
				_pending = false;
				lk.unlock();
				busy = true;
				DoRefresh();
				busy = false;
				lk.lock();
			}
		}

		std::mutex                            _m;
		std::condition_variable               _cv;
		bool                                  _pending{ false };
		std::chrono::steady_clock::time_point _due{};
	} g_worker;

	class EquipSink : public RE::BSTEventSink<RE::TESEquipEvent>
	{
	public:
		RE::BSEventNotifyControl ProcessEvent(const RE::TESEquipEvent* a_event, RE::BSTEventSource<RE::TESEquipEvent>*) override
		{
			if (a_event && a_event->actor && a_event->actor->IsPlayerRef() && !g_worker.busy && GetFreeState()) {
				g_worker.Request(false);
			}
			return RE::BSEventNotifyControl::kContinue;
		}
	} g_equipSink;

	class InputSink : public RE::BSTEventSink<RE::InputEvent*>
	{
	public:
		RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_event, RE::BSTEventSource<RE::InputEvent*>*) override
		{
			if (!a_event) {
				return RE::BSEventNotifyControl::kContinue;
			}
			for (auto* e = *a_event; e; e = e->next) {
				if (e->GetEventType() != RE::INPUT_EVENT_TYPE::kButton || e->GetDevice() != RE::INPUT_DEVICE::kKeyboard) {
					continue;
				}
				auto* button = e->AsButtonEvent();
				if (button->IsDown() && button->GetIDCode() == g_settings.hotkey && !g_worker.busy && GetFreeState()) {
					g_worker.Request(true);
				}
			}
			return RE::BSEventNotifyControl::kContinue;
		}
	} g_inputSink;

	void OnMessage(SKSE::MessagingInterface::Message* a_msg)
	{
		if (a_msg->type != SKSE::MessagingInterface::kDataLoaded) {
			return;
		}
		if (g_settings.autoOnEquip) {
			RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink<RE::TESEquipEvent>(&g_equipSink);
		}
		if (g_settings.hotkey != 0) {
			RE::BSInputDeviceManager::GetSingleton()->AddEventSink(&g_inputSink);
		}
		g_worker.Start();
		SKSE::log::info("ready");
	}
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	SKSE::Init(a_skse);
	g_settings.Load();
	SKSE::GetMessagingInterface()->RegisterListener(OnMessage);
	return true;
}
