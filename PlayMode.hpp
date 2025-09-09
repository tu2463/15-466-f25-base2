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

	// --- input tracking ---
	struct Button {
		uint8_t downs = 0;
		uint8_t pressed = 0;
	} left, right, down, up;

	// --- scoring ---
	int   score = 0;

	// airborne bookkeeping:
	bool  jumper_airborne_prev = false;   // was jumper airborne last frame?
	bool  jumper_airborne      = false;   // jumper_airborne = (jump_z > eps)
	bool  rope_passed  = false;   // did we see a rope pass while airborne?

	// pass detection w.r.t. "under-foot" angle:
	float theta_under      = float(M_PI);            // 12 o’clock is 0 → 6 o’clock (down) is π
	float prev_delta_under = 0.0f;                   // previous wrapped delta for zero-cross
	float pass_eps         = glm::radians(3.0f);     // ignore tiny noise around zero

	// collision heuristic (rope hits ankles):
	float collide_window   = glm::radians(10.0f);    // near the under-foot angle
	float foot_clearance   = 0.10f;                  // rope height threshold (scene units)
	float collision_cooldown = 0.0f;                 // debounce resets (seconds)

	// --- jumper ---
	Scene::Transform* jumper = nullptr;        // transform named "Jumper"
	glm::vec3         jumper_base_position = {};    // initial position

	// Credit: based on Stack Overflow discussion on basic gravity jump physics: https://stackoverflow.com/questions/55373206/basic-gravity-implementation-in-opengl
	float jump_g    = 20.0f;  // m/s^2-ish scene units
	float jump_T    = 1.0f;   // seconds per full jump
	float jump_v0   = 0.0f;   // computed in ctor
	float jump_vz   = 0.0f;   // current vertical velocity
	float jump_z    = 0.0f;   // height above base (meters/scene units)

	// --- rope ---
	Scene::Transform* rope = nullptr;     // transform named "Rope"
	Scene::Transform* left_anchor = nullptr;   // e.g., "Blob1"
	Scene::Transform* right_anchor = nullptr;  // e.g., "Blob2"

	// Credit: Used ChatGPT to help me with the math for rope rotation.
	glm::quat rope_base_rotation = glm::quat(1,0,0,0); // saved initial rotation
	float      rope_theta = 0.0f;                 // current angle around X (radians)
	float      rope_theta_target = 0.0f;          // target angle around X (radians)
	float      rope_slew_rate = glm::radians(360.0f); // max change/sec toward target

	// --- Panel control (screen-space overlay in DrawLines coords) ---
	// Credit: Used ChatGPT to help me with the math here.
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

	//camera:
	Scene::Camera *camera = nullptr;

};
