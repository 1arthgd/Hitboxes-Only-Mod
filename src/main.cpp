#include <Geode/Geode.hpp>
#include <Geode/ui/GeodeUI.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/CCKeyboardDispatcher.hpp>

using namespace geode::prelude;

static constexpr int HITBOX_NODE_TAG = 0x48425858;

// ---------------------------------------------------------------- helpers

static bool isEnabled() {
	return Mod::get()->getSavedValue<bool>("enabled", true);
}

enum class Kind { None, Solid, Hazard, Portal, Gameplay };

static Kind classify(GameObject* obj) {
	switch (obj->m_objectType) {
		case GameObjectType::Solid:
		case GameObjectType::Slope:
			return Kind::Solid;

		case GameObjectType::Hazard:
			return Kind::Hazard;

		case GameObjectType::InverseGravityPortal:
		case GameObjectType::NormalGravityPortal:
		case GameObjectType::ShipPortal:
		case GameObjectType::CubePortal:
		case GameObjectType::InverseMirrorPortal:
		case GameObjectType::NormalMirrorPortal:
		case GameObjectType::BallPortal:
		case GameObjectType::RegularSizePortal:
		case GameObjectType::MiniSizePortal:
		case GameObjectType::UfoPortal:
		case GameObjectType::DualPortal:
		case GameObjectType::SoloPortal:
		case GameObjectType::WavePortal:
		case GameObjectType::RobotPortal:
		case GameObjectType::TeleportPortal:
		case GameObjectType::SpiderPortal:
		case GameObjectType::SwingPortal:
			return Kind::Portal;

		case GameObjectType::YellowJumpPad:
		case GameObjectType::PinkJumpPad:
		case GameObjectType::RedJumpPad:
		case GameObjectType::GravityPad:
		case GameObjectType::YellowJumpRing:
		case GameObjectType::PinkJumpRing:
		case GameObjectType::RedJumpRing:
		case GameObjectType::GravityRing:
		case GameObjectType::GreenRing:
		case GameObjectType::CustomRing:
		case GameObjectType::DashRing:
		case GameObjectType::GravityDashRing:
		case GameObjectType::SecretCoin:
		case GameObjectType::UserCoin:
			return Kind::Gameplay;

		default:
			return Kind::None;
	}
}

static ccColor4F toF(ccColor4B c, float alphaMul = 1.f) {
	return { c.r / 255.f, c.g / 255.f, c.b / 255.f, (c.a / 255.f) * alphaMul };
}

static void drawRect(CCDrawNode* node, CCRect const& r, ccColor4B c, float fill) {
	CCPoint v[4] = {
		{ r.getMinX(), r.getMinY() }, { r.getMaxX(), r.getMinY() },
		{ r.getMaxX(), r.getMaxY() }, { r.getMinX(), r.getMaxY() }
	};
	node->drawPolygon(v, 4, toF(c, fill), 0.6f, toF(c));
}

static void drawCircle(CCDrawNode* node, CCPoint center, float radius, ccColor4B c, float fill) {
	constexpr int N = 24;
	CCPoint v[N];
	for (int i = 0; i < N; i++) {
		float a = (float)i / N * 6.28318530718f;
		v[i] = { center.x + std::cos(a) * radius, center.y + std::sin(a) * radius };
	}
	node->drawPolygon(v, N, toF(c, fill), 0.6f, toF(c));
}

// ---------------------------------------------------------------- gameplay

class $modify(HitboxOnlyPlayLayer, PlayLayer) {
	struct Fields {
		bool m_wasOn = false;
	};

	void setWorldVisible(bool visible) {
		if (m_objectLayer) {
			if (auto children = m_objectLayer->getChildren()) {
				for (auto child : CCArrayExt<CCNode*>(children)) {
					if (child->getTag() != HITBOX_NODE_TAG) child->setVisible(visible);
				}
			}
			if (auto node = m_objectLayer->getChildByTag(HITBOX_NODE_TAG)) {
				node->setVisible(true);
				if (visible) static_cast<CCDrawNode*>(node)->clear();
			}
		}
		if (m_background) m_background->setVisible(visible);
		if (m_middleground) m_middleground->setVisible(visible);
		if (m_groundLayer) m_groundLayer->setVisible(visible);
		if (m_groundLayer2) m_groundLayer2->setVisible(visible);
	}

	void postUpdate(float dt) {
		PlayLayer::postUpdate(dt);

		if (!m_objectLayer) return;

		if (!isEnabled()) {
			if (m_fields->m_wasOn) {
				this->setWorldVisible(true);
				m_fields->m_wasOn = false;
			}
			return;
		}
		m_fields->m_wasOn = true;

		auto node = static_cast<CCDrawNode*>(m_objectLayer->getChildByTag(HITBOX_NODE_TAG));
		if (!node) {
			node = CCDrawNode::create();
			node->setTag(HITBOX_NODE_TAG);
			m_objectLayer->addChild(node, 1000);
		}
		node->clear();

		// hide everything except our hitbox node
		this->setWorldVisible(false);

		auto mod = Mod::get();
		bool showPortals = mod->getSettingValue<bool>("show-portals");
		bool showGameplay = mod->getSettingValue<bool>("show-gameplay");
		float fill = static_cast<float>(mod->getSettingValue<double>("fill-opacity"));
		auto cPlayer = mod->getSettingValue<ccColor4B>("player-color");
		auto cSolid = mod->getSettingValue<ccColor4B>("solid-color");
		auto cHazard = mod->getSettingValue<ccColor4B>("hazard-color");
		auto cPortal = mod->getSettingValue<ccColor4B>("portal-color");
		auto cGameplay = mod->getSettingValue<ccColor4B>("gameplay-color");

		// objects (simple horizontal culling around the player)
		float range = CCDirector::get()->getWinSize().width * 1.5f;
		float px = m_player1 ? m_player1->getPositionX() : 0.f;

		if (m_objects) {
			for (auto obj : CCArrayExt<GameObject*>(m_objects)) {
				if (!obj) continue;
				if (std::abs(obj->getPositionX() - px) > range) continue;

				Kind kind = classify(obj);
				if (kind == Kind::None) continue;
				if (kind == Kind::Portal && !showPortals) continue;
				if (kind == Kind::Gameplay && !showGameplay) continue;

				ccColor4B color = cSolid;
				switch (kind) {
					case Kind::Hazard: color = cHazard; break;
					case Kind::Portal: color = cPortal; break;
					case Kind::Gameplay: color = cGameplay; break;
					default: break;
				}

				if (obj->m_objectRadius > 0.f) {
					drawCircle(node, obj->getPosition(), obj->m_objectRadius, color, fill);
				} else {
					drawRect(node, obj->getObjectRect(), color, fill);
				}
			}
		}

		// players
		if (m_player1) drawRect(node, m_player1->getObjectRect(), cPlayer, fill);
		if (m_player2 && m_gameState.m_isDualMode) drawRect(node, m_player2->getObjectRect(), cPlayer, fill);
	}
};

// ---------------------------------------------------------------- H key

class $modify(HitboxOnlyKeys, CCKeyboardDispatcher) {
	bool dispatchKeyboardMSG(enumKeyCodes key, bool down, bool repeat, double timestamp) {
		if (down && !repeat && key == enumKeyCodes::KEY_H) {
			if (PlayLayer::get()) {
				bool now = !isEnabled();
				Mod::get()->setSavedValue("enabled", now);
				Notification::create(
					now ? "Hitbox Only: ON" : "Hitbox Only: OFF",
					now ? NotificationIcon::Success : NotificationIcon::Info,
					0.8f
				)->show();
				return true;
			}
			auto scene = CCDirector::get()->getRunningScene();
			if (scene && scene->getChildByType<MenuLayer>(0)) {
				openSettingsPopup(Mod::get(), false);
				return true;
			}
		}
		return CCKeyboardDispatcher::dispatchKeyboardMSG(key, down, repeat, timestamp);
	}
};
