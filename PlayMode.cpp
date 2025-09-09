#include "PlayMode.hpp"

#include "LitColorTextureProgram.hpp"

#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <random>

static constexpr glm::vec3 WORLD_Y = glm::vec3(0.0f, 1.0f, 0.0f);
static constexpr glm::vec3 WORLD_Z = glm::vec3(0.0f, 0.0f, 1.0f);

namespace
{
	// pixel → overlay (DrawLines) coords: x ∈ [-aspect,+aspect], y ∈ [-1,+1]
	inline glm::vec2 pixel_to_overlay(glm::uvec2 win, glm::ivec2 px)
	{
		float aspect = float(win.x) / float(win.y);
		float ox = ((float(px.x) / float(win.x)) * 2.0f - 1.0f) * aspect;
		float oy = 1.0f - (float(px.y) / float(win.y)) * 2.0f;
		return glm::vec2(ox, oy);
	}

	// clamp a point to a disk of radius R around C (overlay coords)
	inline glm::vec2 clamp_to_disk(glm::vec2 p, glm::vec2 c, float R)
	{
		glm::vec2 v = p - c;
		float d = glm::length(v);
		if (d <= R || d == 0.0f)
			return p;
		return c + v * (R / d);
	}

	// signed angle with 12 o'clock = 0 and clockwise positive:
	// top (0,+R) -> 0; right (+R,0) -> +pi/2; bottom (0,-R) -> ±pi; left (-R,0) -> -pi/2
	inline float angle_from_top_cw(glm::vec2 v)
	{
		return std::atan2(v.x, v.y); // thanks to overlay +Y up, this gives desired mapping
	}

	// shortest wrapped delta to [-pi,pi]
	// inline float wrap_pi(float a) { return std::atan2(std::sin(a), std::cos(a)); }
} // namespace

GLuint rope_meshes_for_lit_color_texture_program = 0;

Load< MeshBuffer > rope_meshes(LoadTagDefault, []() -> MeshBuffer const * {
    // NOTE: adjust path if your Makefile writes elsewhere:
    MeshBuffer const *ret = new MeshBuffer(data_path("ropegame.pnct"));
    rope_meshes_for_lit_color_texture_program =
        ret->make_vao_for_program(lit_color_texture_program->program);
    return ret;
});

Load< Scene > rope_scene(LoadTagDefault, []() -> Scene const * {
    // If you don't export a .scene yet, see the "No .scene file" option below.
    return new Scene(data_path("ropegame.scene"),
        [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name) {
            Mesh const &mesh = rope_meshes->lookup(mesh_name);

            Scene::Drawable &dr = scene.drawables.emplace_back(transform);
            dr.pipeline = lit_color_texture_program_pipeline;
            dr.pipeline.vao   = rope_meshes_for_lit_color_texture_program;
            dr.pipeline.type  = mesh.type;
            dr.pipeline.start = mesh.start;
            dr.pipeline.count = mesh.count;
        }
    );
});

PlayMode::PlayMode() : scene(*rope_scene) {
	//get pointer to camera for convenience:
	if (scene.cameras.size() != 1) throw std::runtime_error("Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));
	camera = &scene.cameras.front();

	//get pointer to Jumper for convenience:
	// Credit: imitate the starter code
	for (auto &transform : scene.transforms) {
		if (transform.name == "Jumper") jumper = &transform;
		else if (transform.name == "Blob1") left_anchor = &transform;
		else if (transform.name == "Blob2") right_anchor = &transform;
    	else if (transform.name == "Rope")  rope = &transform;
	}
	if (!jumper) throw std::runtime_error("Jumper not found.");
	if (!left_anchor || !right_anchor) throw std::runtime_error("Anchors Blob1/Blob2 not found.");
	if (!rope) throw std::runtime_error("Rope transform not found.");

	// remember jumper and rope's initial position/orientation so we can move them on top of these values
	jumper_base_position = jumper->position;
	rope_base_rotation = rope->rotation;
	rope->position = 0.5f * (left_anchor->position + right_anchor->position);

	// Credit: Stack Overflow discussion on basic gravity jump physics: https://stackoverflow.com/questions/55373206/basic-gravity-implementation-in-opengl
	jump_v0 = 0.5f * jump_g * jump_T; 
	jump_vz = jump_v0;
	jump_z  = 0.0f;
}

PlayMode::~PlayMode() {
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {
    float aspect = float(window_size.x) / float(window_size.y);

    // Place the panel in bottom-right with a margin:
    panel_center = glm::vec2( aspect - (panel_margin + panel_radius),
                             -1.0f    + (panel_margin + panel_radius) );

    if (evt.type == SDL_EVENT_MOUSE_BUTTON_DOWN && evt.button.button == SDL_BUTTON_LEFT) {
        glm::vec2 m_ol = pixel_to_overlay(window_size, {evt.button.x, evt.button.y});
        float R = panel_radius;
        if (glm::length(m_ol - panel_center) <= R) {
            panel_dragging = true;
            handle_pos_ol = clamp_to_disk(m_ol, panel_center, R);
            glm::vec2 v = handle_pos_ol - panel_center;
            float dead = panel_dead_frac * R;
            if (glm::length(v) >= dead) {
                panel_angle = angle_from_top_cw(v);
                rope_theta_target = -panel_angle; // direct mapping: panel angle = rope target
            }
            return true;
        }
    }
    else if (evt.type == SDL_EVENT_MOUSE_BUTTON_UP && evt.button.button == SDL_BUTTON_LEFT) {
        panel_dragging = false;
        return true;
    }
    else if (evt.type == SDL_EVENT_MOUSE_MOTION) {
        glm::vec2 m_ol = pixel_to_overlay(window_size, {evt.motion.x, evt.motion.y});
        float R = panel_radius;

        // if dragging, the handle follows the cursor; otherwise leave it in-place
        if (panel_dragging) {
            handle_pos_ol = clamp_to_disk(m_ol, panel_center, R);
            glm::vec2 v = handle_pos_ol - panel_center;
            float dead = panel_dead_frac * R;
            if (glm::length(v) >= dead) {
                panel_angle = angle_from_top_cw(v);
                rope_theta_target = -panel_angle;
            }
            return true;
        }
    }

    return false;
}

void PlayMode::update(float elapsed)
{
	// --- Jumper: gravity-based ballistic jump (exactly 1 Hz) ---
	if (jumper) {
		// substep if needed to keep integration stable on large elapsed:
		float remaining = elapsed;
		while (remaining > 0.0f) {
			float dt = std::min(remaining, 1.0f / 120.0f); // up to 120 Hz substeps

			jump_vz -= jump_g * dt;     // v = v + a * dt (a = -g) = v - g * dt
			jump_z  += jump_vz * dt;    // z = z + v * dt

			if (jump_z <= 0.0f) {       // landed: clamp & relaunch
				jump_z = 0.0f;
				jump_vz = jump_v0;      // restart next hop
			}
			remaining -= dt;
		}
		jumper->position = jumper_base_position + WORLD_Z * jump_z;
	}

	// --- Rope (rotation around x, towards cursor) ---
    if (rope && left_anchor && right_anchor) {
        const glm::vec3 mid = 0.5f * (left_anchor->position + right_anchor->position);
        rope->position = mid;

		// Credit: Used ChatGPT to help me with the math to create the rope rotation.
        float err = rope_theta_target - rope_theta;
        err = std::atan2(std::sin(err), std::cos(err)); // wrap to [-pi,pi] to get shortest path:
        float max_step = rope_slew_rate * elapsed;
        float step = glm::clamp(err, -max_step, max_step);
        rope_theta += step;
		rope->rotation = rope_base_rotation * glm::angleAxis(rope_theta, WORLD_Y);
		// printf("rope->rotation: (%f, %f, %f, %f)\n", rope->rotation.w, rope->rotation.x, rope->rotation.y, rope->rotation.z);
	}

	// --- scoring / pass + collision detection ---
	{
		jumper_airborne = (jump_z > 0.1f);
		if (!jumper_airborne_prev && jumper_airborne) {
			rope_passed  = false;  // new jump just started
		}

		//?? some mathmatical calculation might be here involving theta_under, delta_under and prev_delta_under.
		// current wrapped delta to "under-foot" (6 o'clock) angle:
		float delta_under = std::atan2(std::sin(rope_theta - theta_under),
									   std::cos(rope_theta - theta_under));
		// printf("jump_z: %f, jumper_airborne: %d, jumper_airborne_prev: %d, rope_passed: %d, rope_theta (deg): %f, theta_under (deg): %f, delta_under (deg): %f\n",
		// 	   jump_z, jumper_airborne, jumper_airborne_prev, rope_passed,
		// 	   glm::degrees(rope_theta), glm::degrees(theta_under), glm::degrees(delta_under));

		if (jumper_airborne && !rope_passed) // when in air
		{
			// Credit: used ChatGPT to help me with the math to detect whether the rope passed under the jumper.
			

			bool sign_cross = (delta_under > 0.0f && prev_delta_under <= 0.0f) ||
							  (delta_under < 0.0f && prev_delta_under >= 0.0f);

			// is this sign flip happening up near the ±π boundary (i.e., over the head)?
			bool near_pi = (std::abs(prev_delta_under) > float(M_PI) - near_pi_window) &&
						   (std::abs(delta_under) > float(M_PI) - near_pi_window);

			rope_passed = sign_cross && !near_pi; // genuine pass under feet
		}

		if (jumper_airborne_prev && !jumper_airborne) // just landed
		{
			// printf(" --- LANDED: %d\n", score);
			if (rope_passed) {
				++score; 
				// printf(" --- SCORE: %d\n", score);
			}
			rope_passed = false;
		}

		// collision heuristic: if rope is near under-foot angle AND feet are low -> reset score
		// debounce with a small cooldown so we don't spam resets across a few frames
		if (collision_cooldown > 0.0f)
			collision_cooldown -= elapsed;
		if (std::abs(delta_under) < collide_window && jump_z <= foot_clearance)
		{
			if (collision_cooldown <= 0.0f)
			{
				score = 0;
				collision_cooldown = 0.30f; // seconds
			}
		}

		// remember for next frame:
		jumper_airborne_prev = jumper_airborne;
		prev_delta_under = delta_under;
	}

	// reset button press counters:
	left.downs = 0;
	right.downs = 0;
	up.downs = 0;
	down.downs = 0;
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
	//update camera aspect ratio for drawable:
	camera->aspect = float(drawable_size.x) / float(drawable_size.y);

	//set up light type and position for lit_color_texture_program:
	// TODO: consider using the Light(s) in the scene to do this
	glUseProgram(lit_color_texture_program->program);
	glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 1);
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f,-1.0f)));
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.95f)));
	glUseProgram(0);

	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.

	GL_ERRORS(); //print any errors produced by this setup code

	scene.draw(*camera);

	{ //use DrawLines to overlay instructions:
		glDisable(GL_DEPTH_TEST);
		float aspect = float(drawable_size.x) / float(drawable_size.y);
		DrawLines lines(glm::mat4(
			1.0f / aspect, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		));

		constexpr float H = 0.09f;
		lines.draw_text("Click and drag in the bottom right circle to sway the rope. Try to jump more continuously!", // any better ideas for this instruction? need to be no longer than this
			glm::vec3(-aspect + 0.1f * H, -1.0 + 0.1f * H, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		float ofs = 2.0f / drawable_size.y;
		lines.draw_text("Click and drag in the bottom right circle to sway the rope. Try to jump more continuously",
						glm::vec3(-aspect + 0.1f * H + ofs, -1.0 + 0.1f * H + ofs, 0.0),
						glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
						glm::u8vec4(0xff, 0xff, 0xff, 0x00));
	}

	{ //use DrawLines to overlay the control panel and handle
		glDisable(GL_DEPTH_TEST);
		// Credit: Used ChatGPT to help me with the math to get the handle rotation angle and map to rope rotation angle.
		float aspect = float(drawable_size.x) / float(drawable_size.y);
		DrawLines lines(glm::mat4(
			1.0f / aspect, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f));

		// compute center every frame (matches handle_event)
		panel_center = glm::vec2(aspect - (panel_margin + panel_radius),
								 -1.0f + (panel_margin + panel_radius));
		float R = panel_radius;

		glm::u8vec4 ring_col = glm::u8vec4(0x22, 0x22, 0x22, 0xff);
		glm::u8vec4 ref_col = glm::u8vec4(0xdd, 0x66, 0x66, 0xff); // 12 o'clock ray
		glm::u8vec4 handle_col = glm::u8vec4(0xff, 0xff, 0xff, 0xff);
		glm::u8vec4 guide_col = glm::u8vec4(0x88, 0x88, 0x88, 0xff);

		// ring (approximate circle)
		const int N = 64;
		for (int i = 0; i < N; ++i)
		{
			float a0 = (float(i) / N) * 2.0f * float(M_PI);
			float a1 = (float(i + 1) / N) * 2.0f * float(M_PI);
			glm::vec3 p0 = glm::vec3(panel_center + R * glm::vec2(std::cos(a0), std::sin(a0)), 0.0f);
			glm::vec3 p1 = glm::vec3(panel_center + R * glm::vec2(std::cos(a1), std::sin(a1)), 0.0f);
			lines.draw(p0, p1, ring_col);
		}

		// 12 o'clock reference ray
		glm::vec3 c3 = glm::vec3(panel_center, 0.0f);
		glm::vec3 top = glm::vec3(panel_center + glm::vec2(0.0f, R), 0.0f);
		lines.draw(c3, top, ref_col);

		// current angle ray (from center to handle; recompute if handle never moved)
		glm::vec2 hp = handle_pos_ol;
		if (!panel_dragging && glm::length(hp - panel_center) < 1e-4f)
		{
			// place the handle at current rope target if not yet moved:
			glm::vec2 v = glm::vec2(std::sin(-rope_theta_target), std::cos(-rope_theta_target)); // inverse of angle_from_top_cw
			hp = panel_center + v * (R * 0.85f);
		}
		lines.draw(c3, glm::vec3(hp, 0.0f), guide_col);

		// draw the handle as a small circle:
		float rH = R * 0.08f;
		for (int i = 0; i < N; ++i)
		{
			float a0 = (float(i) / N) * 2.0f * float(M_PI);
			float a1 = (float(i + 1) / N) * 2.0f * float(M_PI);
			glm::vec3 p0 = glm::vec3(hp + rH * glm::vec2(std::cos(a0), std::sin(a0)), 0.0f);
			glm::vec3 p1 = glm::vec3(hp + rH * glm::vec2(std::cos(a1), std::sin(a1)), 0.0f);
			lines.draw(p0, p1, handle_col);
		}
	}

	{ // use DrawLines to overlay scores
		glDisable(GL_DEPTH_TEST);
		// Credit: Used ChatGPT to draw the score at the correct top right position.
		float aspect = float(drawable_size.x) / float(drawable_size.y);
		DrawLines lines(glm::mat4(
			1.0f / aspect, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f));

		const float H = 0.10f; // scale
		// anchor near top-right; we don't have text width, so just place with margin
		glm::vec3 pos = glm::vec3(aspect - panel_margin - 6.0f * H,
								  1.0f - panel_margin - 1.2f * H,
								  0.0f);
		std::string text = "Score: " + std::to_string(score);

		// shadow
		float ofs = 2.0f / drawable_size.y;
		lines.draw_text(text,
						pos + glm::vec3(ofs, ofs, 0.0f),
						glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
						glm::u8vec4(0x00, 0x00, 0x00, 0x00));

		// main
		lines.draw_text(text,
						pos,
						glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
						glm::u8vec4(0xff, 0xff, 0xff, 0x00));
	}
}
