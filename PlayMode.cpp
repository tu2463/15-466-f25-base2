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
}

PlayMode::~PlayMode() {
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

	if (evt.type == SDL_EVENT_KEY_DOWN) {
		if (evt.key.key == SDLK_A) {
			left.downs += 1;
			left.pressed = true;
			return true;
		} else if (evt.key.key == SDLK_D) {
			right.downs += 1;
			right.pressed = true;
			return true;
		} else if (evt.key.key == SDLK_W) {
			up.downs += 1;
			up.pressed = true;
			return true;
		} else if (evt.key.key == SDLK_S) {
			down.downs += 1;
			down.pressed = true;
			return true;
		}
	} else if (evt.type == SDL_EVENT_KEY_UP) {
		if (evt.key.key == SDLK_A) {
			left.pressed = false;
			return true;
		} else if (evt.key.key == SDLK_D) {
			right.pressed = false;
			return true;
		} else if (evt.key.key == SDLK_W) {
			up.pressed = false;
			return true;
		} else if (evt.key.key == SDLK_S) {
			down.pressed = false;
			return true;
		}
	}  else if (evt.type == SDL_EVENT_MOUSE_MOTION) {
		// if (SDL_GetWindowRelativeMouseMode(Mode::window) == true) {
			// glm::vec2 motion = glm::vec2(
			// 	evt.motion.xrel / float(window_size.y),
			// 	-evt.motion.yrel / float(window_size.y)
			// );
			// camera->transform->rotation = glm::normalize(
			// 	camera->transform->rotation
			// 	* glm::angleAxis(-motion.x * camera->fovy, glm::vec3(0.0f, 1.0f, 0.0f))
			// 	* glm::angleAxis( motion.y * camera->fovy, glm::vec3(1.0f, 0.0f, 0.0f))
			// );

			// accumulate a horizontal “cursor” in [-1,1] from xrel: //??
			cursor_x_norm += (evt.motion.xrel / float(window_size.x)) * 2.0f; // sensitivity
			cursor_x_norm = glm::clamp(cursor_x_norm, -1.0f, 1.0f);

			// map cursor to target tilt around X:
			rope_theta_target = glm::clamp(cursor_x_norm, -1.0f, 1.0f) * rope_max_tilt;

			// // Use vertical instead of horizontal to feel “down/up”:
			// cursor_x_norm += (-evt.motion.yrel / float(window_size.y)) * 2.0f; // negative so moving down tilts down
			// cursor_x_norm  = glm::clamp(cursor_x_norm, -1.0f, 1.0f);

			// // Allow deeper tilt:
			// rope_max_tilt = glm::radians(170.0f);  // was 80.0f
			// rope_theta_target = cursor_x_norm * rope_max_tilt;


			return true;
		// }
	}

	return false;
}

void PlayMode::update(float elapsed) {
	//move camera:
	{

		//combine inputs into a move:
		constexpr float PlayerSpeed = 30.0f;
		glm::vec2 move = glm::vec2(0.0f);
		if (left.pressed && !right.pressed) move.x =-1.0f;
		if (!left.pressed && right.pressed) move.x = 1.0f;
		if (down.pressed && !up.pressed) move.y =-1.0f;
		if (!down.pressed && up.pressed) move.y = 1.0f;

		//make it so that moving diagonally doesn't go faster:
		if (move != glm::vec2(0.0f)) move = glm::normalize(move) * PlayerSpeed * elapsed;

		glm::mat4x3 frame = camera->transform->make_parent_from_local();
		glm::vec3 frame_right = frame[0];
		//glm::vec3 up = frame[1];
		glm::vec3 frame_forward = -frame[2];

		camera->transform->position += move.x * frame_right + move.y * frame_forward;
	}

	// --- Jumper (rise/fall once per second) ---
	{
		if (jumper) {
			// Credit: Used ChatGPT to help me with the math to create jumping animation.
			jumper_time += elapsed;                      // accumulate seconds
			float phase = jumper_time - std::floor(jumper_time); // [0,1)
			// half-sine: feet on ground for half the cycle, airborne for half:
    		float z_offset = jumper_amp * std::max(0.0f, std::sin(2.0f * float(M_PI) * phase));
			jumper->position = jumper_base_position + WORLD_Z * z_offset;
		}
	}

	// --- Rope (rotation around x, towards cursor) ---
	// 1) keep rope transform centered between anchors:
    if (rope && left_anchor && right_anchor) {
        const glm::vec3 mid = 0.5f * (left_anchor->position + right_anchor->position);
        rope->position = mid;

        // 2) slew rope_theta -> rope_theta_target with a max speed:
        float err = rope_theta_target - rope_theta;
        // wrap to [-pi,pi] to get shortest path (optional but nice):
        err = std::atan2(std::sin(err), std::cos(err));
        float max_step = rope_slew_rate * elapsed;
        float step = glm::clamp(err, -max_step, max_step);
        rope_theta += step;

        // 3) rotate around Y
        rope->rotation = rope_base_rotation * glm::angleAxis(rope_theta, WORLD_Y);
		printf("rope->rotation: (%f, %f, %f, %f)\n", rope->rotation.w, rope->rotation.x, rope->rotation.y, rope->rotation.z);
    }

	//reset button press counters:
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

	{ //use DrawLines to overlay some text:
		glDisable(GL_DEPTH_TEST);
		float aspect = float(drawable_size.x) / float(drawable_size.y);
		DrawLines lines(glm::mat4(
			1.0f / aspect, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		));

		constexpr float H = 0.09f;
		lines.draw_text("Mouse motion rotates camera; WASD moves; escape ungrabs mouse",
			glm::vec3(-aspect + 0.1f * H, -1.0 + 0.1f * H, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		float ofs = 2.0f / drawable_size.y;
		lines.draw_text("Mouse motion rotates camera; WASD moves; escape ungrabs mouse",
			glm::vec3(-aspect + 0.1f * H + ofs, -1.0 + 0.1f * H + ofs, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));
	}
}
