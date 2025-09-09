#include "Mode.hpp"

#include "Scene.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <deque>

struct PlayMode : Mode {
	PlayMode();
	virtual ~PlayMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	//----- game state -----

	//input tracking:
	struct Button {
		uint8_t downs = 0;
		uint8_t pressed = 0;
	} left, right, down, up;

	// --- jumper ---
	Scene::Transform* jumper = nullptr;        // transform named "Jumper"
	glm::vec3         jumper_base_position = {};    // initial position
	float             jumper_time = 0.0f;      // accumulative time (s)
	float             jumper_amp  = 3.0f;      // jump height

	// --- rope ---
	Scene::Transform* rope = nullptr;     // transform named "Rope"
	Scene::Transform* left_anchor = nullptr;   // e.g., "Blob1"
	Scene::Transform* right_anchor = nullptr;  // e.g., "Blob2"

	glm::quat rope_base_rotation = glm::quat(1,0,0,0); // saved initial rotation
	float      rope_theta = 0.0f;                 // current angle around X (radians)
	float      rope_theta_target = 0.0f;          // target angle around X (radians)
	float      rope_slew_rate = glm::radians(360.0f); // max change/sec toward target
	float      cursor_x_norm = 0.0f;              // [-1,1] from mouse motion accumulation

	float      rope_max_tilt = glm::radians(360.0f);  // clamp target angle

	// --- Panel control (screen-space overlay in DrawLines coords) ---
	glm::vec2 panel_center = glm::vec2(0.0f); // computed each frame from window aspect
	float     panel_radius = 0.22f;           // overlay units (x in [-aspect,+aspect], y in [-1,+1])
	float     panel_margin = 0.10f;           // distance from screen edge (overlay units)

	bool      panel_dragging = false;         // left mouse held on panel
	glm::vec2 handle_pos_ol = glm::vec2(0.0f); // handle position in overlay coords
	float     panel_angle   = 0.0f;           // radians, 12 o'clock = 0, clockwise positive

	// (optional) ignore noisy motion near center:
	float     panel_dead_frac = 0.06f;        // deadzone radius as fraction of panel_radius

	//local copy of the game scene (so code can change it during gameplay):
	Scene scene;

	//hexapod leg to wobble:
	// Scene::Transform *hip = nullptr;
	// Scene::Transform *upper_leg = nullptr;
	// Scene::Transform *lower_leg = nullptr;
	// glm::quat hip_base_rotation;
	// glm::quat upper_leg_base_rotation;
	// glm::quat lower_leg_base_rotation;
	// float wobble = 0.0f;
	
	//camera:
	Scene::Camera *camera = nullptr;

};
