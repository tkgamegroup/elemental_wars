#include <flame/universe/application.h>

#include <flame/xml.h>
#include <flame/foundation/system.h>
#include <flame/graphics/canvas.h>
#include <flame/universe/components/element.h>
#include <flame/universe/components/image.h>
#include <flame/universe/components/receiver.h>
#include <flame/universe/components/camera.h>

struct Game : UniverseApplication
{
	cCameraPtr camera = nullptr;

	void init();
	bool on_update() override;
	void on_render() override;
	void on_gui() override;
};

const auto tile_cx = 60U;
const auto tile_cy = 30U;
const auto tile_sz = 32.f;
const auto tile_sz_y = tile_sz * 0.5f * 1.7320508071569;
graphics::ImagePtr img_tile = nullptr;
graphics::ImagePtr img_tile_select = nullptr;
graphics::ImagePtr img_building = nullptr;

enum ElementType
{
	ElementFire,
	ElementWater,
	ElementGrass
};

enum BuildingType
{
	BuildingElementCollector,
	BuildingFireTower,
	BuildingWaterTower,
	BuildingGrassTower,
	BuildingFireFairyMaker,
	BuildingWaterFairyMaker,
	BuildingGrassFairyMaker,
};

struct cTile : Component
{
	cElementPtr element = nullptr;

	uint id;
	ElementType element_type;

	cTile* tile_lt = nullptr;
	cTile* tile_t = nullptr;
	cTile* tile_rt = nullptr;
	cTile* tile_lb = nullptr;
	cTile* tile_b = nullptr;
	cTile* tile_rb = nullptr;

	cTile() { type_hash = "cTile"_h; }
	virtual ~cTile() {}
};

cElementPtr tile_select = nullptr;

struct cPlayer;

struct cBuilding : Component
{
	cPlayer* player = nullptr;
	cTile* tile = nullptr;

	float t = 0.f;

	cBuilding() { type_hash = "cBuilding"_h; }
	virtual ~cBuilding() {}
};

struct cElementCollector : cBuilding
{
	cElementCollector() { type_hash = "cElementCollector"_h; }
	virtual ~cElementCollector() {}

	void update() override;
};

struct cPlayer : Component
{
	uint fire_element = 0;
	uint water_element = 0;
	uint grass_element = 0;

	EntityPtr buildings = nullptr;

	cPlayer() { type_hash = "cPlayer"_h; }
	virtual ~cPlayer() {}

	void on_active() override
	{
		buildings = Entity::create();
		buildings->name = "buildings";
		buildings->add_component<cElement>();
		entity->add_child(buildings);
	}

	cBuilding* add_building(BuildingType type, cTile* tile)
	{
		cBuilding* building = nullptr;
		switch (type)
		{
		case BuildingElementCollector:
		{
			auto e = Entity::create();
			auto element = e->add_component<cElement>();
			element->pos = tile->element->pos;
			element->ext = vec2(tile_sz, tile_sz_y);
			auto image = e->add_component<cImage>();
			image->image = img_building;
			auto b = new cElementCollector;
			b->player = this;
			b->tile = tile;
			e->add_component_p(b);
			buildings->add_child(e);

			building = b;
		}
			break;
		}
		return building;
	}
};

void cElementCollector::update()
{
	t += delta_time;
	if (t >= 1.f)
	{
		t = 0.f;

		switch (tile->element_type)
		{
		case ElementFire: player->fire_element++; break;
		case ElementWater: player->water_element++; break;
		case ElementGrass: player->grass_element++; break;
		}
	}
}

cPlayer* main_player = nullptr;
cTile* hovering_tile = nullptr;
int selecting_building = -1;

void Game::init()
{
	srand(time(0));

	create("Elemental Wars", uvec2(1280, 720), WindowStyleFrame | WindowStyleResizable, false, true,
		{ {"mesh_shader"_h, 0} });

	Path::set_root(L"assets", L"assets");

	img_tile = graphics::Image::get(L"assets/tile.png");
	img_tile_select = graphics::Image::get(L"assets/tile_select.png");
	img_building = graphics::Image::get(L"assets/building.png");

	auto root = world->root.get();

	auto e_element_root = Entity::create();
	e_element_root->add_component<cElement>();
	root->add_child(e_element_root);

	{
		auto e = Entity::create();
		auto element = e->add_component<cElement>();
		camera = e->add_component<cCamera>();
		camera->pivot = vec2(0.5f);
		e_element_root->add_child(e);
	}

	auto e_tiles_root = Entity::create();
	e_tiles_root->add_component<cElement>();
	e_element_root->add_child(e_tiles_root);
	for (auto y = 0; y < tile_cy; y++)
	{
		for (auto x = 0; x < tile_cx; x++)
		{
			auto id = y * tile_cx + x;
			auto e = Entity::create();
			auto element = e->add_component<cElement>();
			element->ext = vec2(tile_sz, tile_sz_y);
			element->pos = vec2(x * tile_sz * 0.75f, y * tile_sz_y);
			if (x % 2 == 1)
				element->pos.y += tile_sz_y * 0.5f;
			auto image = e->add_component<cImage>();
			image->image = img_tile;
			auto tile = new cTile;
			tile->element = element;
			tile->id = id;
			e->add_component_p(tile);
			switch (linearRand(0, 2))
			{
			case 0: 
				image->tint_col = cvec4(255, 127, 127, 255);
				tile->element_type = ElementFire;
				break;
			case 1: 
				image->tint_col = cvec4(127, 127, 255, 255);
				tile->element_type = ElementWater;
				break;
			case 2:
				image->tint_col = cvec4(127, 255, 127, 255);
				tile->element_type = ElementGrass;
				break;
			}
			auto receiver = e->add_component<cReceiver>();
			receiver->event_listeners.add([tile](uint type,  const vec2& value) {
				switch (type)
				{
				case "mouse_enter"_h:
					hovering_tile = tile;
					break;
				case "mouse_leave"_h:
					if (hovering_tile == tile)
						hovering_tile = nullptr;
					break;
				}
			});
			e_tiles_root->add_child(e);
		}
	}
	for (auto y = 0; y < tile_cy; y++)
	{
		for (auto x = 0; x < tile_cx; x++)
		{
			auto id = y * tile_cx + x;
			auto tile = e_tiles_root->children[id]->get_component<cTile>();

			if (x % 2 == 0)
			{
				if (x > 0 && y > 0)
					tile->tile_lt = e_tiles_root->children[id - tile_cx - 1]->get_component<cTile>();
				if (x < tile_cx - 1 && y > 0)
					tile->tile_rt = e_tiles_root->children[id - tile_cx + 1]->get_component<cTile>();
				if (x > 0 && y < tile_cy - 1)
					tile->tile_lb = e_tiles_root->children[id - 1]->get_component<cTile>();
				if (x < tile_cx - 1 && y < tile_cy - 1)
					tile->tile_rb = e_tiles_root->children[id + 1]->get_component<cTile>();
			}
			else
			{
				tile->tile_lt = e_tiles_root->children[id - 1]->get_component<cTile>();
				if (x < tile_cx - 1)
					tile->tile_rt = e_tiles_root->children[id + 1]->get_component<cTile>();
				if (y < tile_cy - 1)
					tile->tile_lb = e_tiles_root->children[id + tile_cx - 1]->get_component<cTile>();
				if (x < tile_cx - 1 && y < tile_cy - 1)
					tile->tile_rb = e_tiles_root->children[id + tile_cx + 1]->get_component<cTile>();
			}

			if (y > 0)
				tile->tile_t = e_tiles_root->children[id - tile_cx]->get_component<cTile>();
			if (y < tile_cy - 1)
				tile->tile_b = e_tiles_root->children[id + tile_cx]->get_component<cTile>();
		}
	}

	{
		auto p0 = e_tiles_root->first_child()->get_component<cElement>()->pos + vec2(tile_sz) * 0.5f;
		auto p1 = e_tiles_root->last_child()->get_component<cElement>()->pos + vec2(tile_sz) * 0.5f;
		camera->element->set_pos((p0 + p1) * 0.5f);
		camera->restrict_lt = p0;
		camera->restrict_rb = p1;
	}

	{
		auto e = Entity::create();
		auto element = e->add_component<cElement>();
		element->ext = vec2(tile_sz, tile_sz_y);
		auto image = e->add_component<cImage>();
		image->image = img_tile_select;
		image->tint_col = cvec4(255);
		e_element_root->add_child(e);

		e->set_enable(false);
		tile_select = element;
	}

	auto e_players_root = Entity::create();
	e_players_root->add_component<cElement>();
	e_element_root->add_child(e_players_root);

	{
		auto e = Entity::create();
		auto element = e->add_component<cElement>();
		main_player = new cPlayer;
		e->add_component_p(main_player);
		e_players_root->add_child(e);
	}

	renderer->add_render_task(RenderModeShaded, camera, main_window, {}, graphics::ImageLayoutPresent);
}

bool Game::on_update()
{
	if (auto canvas = hud->canvas; canvas)
	{
		auto screen_size = canvas->size;

		hud->begin("top"_h, vec2(0.f, 0.f), vec2(screen_size.x, 24.f), cvec4(0, 0, 0, 255));
		hud->begin_layout(HudHorizontal);
		hud->rect(vec2(16.f), cvec4(255, 127, 127, 255));
		hud->text(std::format(L"{}", main_player->fire_element));
		hud->rect(vec2(16.f), cvec4(127, 127, 255, 255));
		hud->text(std::format(L"{}", main_player->water_element));
		hud->rect(vec2(16.f), cvec4(127, 255, 127, 255));
		hud->text(std::format(L"{}", main_player->grass_element));
		hud->end_layout();
		hud->end();

		auto bottom_pannel_height = 50.f;
		hud->begin("bottom"_h, vec2(0.f, screen_size.y - bottom_pannel_height), vec2(screen_size.x, bottom_pannel_height), cvec4(0, 0, 0, 255));

		{
			auto selected = selecting_building == BuildingElementCollector;
			if (selected)
			{
				hud->push_style_color(HudStyleColorButton, cvec4(35 * 2, 69 * 2, 109 * 2, 255));
				hud->push_style_color(HudStyleColorButtonHovered, cvec4(66 * 2, 200, 255, 255));
			}
			if (hud->button(L"Element Collector"))
				selecting_building = BuildingElementCollector;
			if (selected)
			{
				hud->pop_style_color(HudStyleColorButton);
				hud->pop_style_color(HudStyleColorButtonHovered);
			}
		}

		if (hovering_tile)
		{
			tile_select->entity->set_enable(true);
			tile_select->set_pos(hovering_tile->element->pos);
		}
		else
			tile_select->entity->set_enable(false);

		hud->end();
	}

	UniverseApplication::on_update();

	if (input->mbtn[Mouse_Right])
	{
		camera->element->add_pos(-input->mdisp);
		selecting_building = -1;
	}
	else if (input->mpressed(Mouse_Left))
	{
		if (selecting_building != -1 && hovering_tile)
		{
			main_player->add_building((BuildingType)selecting_building, hovering_tile);
		}
	}

	if (input->mscroll != 0)
	{
		static float scales[] = { 1.f, 1.2f, 1.4f, 1.6f, 1.8f, 2.f, 2.5f, 3.f };
		auto scl = camera->element->scl.x;
		if (input->mscroll > 0)
		{
			auto it = std::find(scales, scales + count_of(scales), scl);
			if (it + 1 != scales + count_of(scales))
				camera->element->set_scl(vec2(*(it + 1)));
		}
		if (input->mscroll < 0)
		{
			auto it = std::find(scales, scales + count_of(scales), scl);
			if (it != scales)
				camera->element->set_scl(vec2(*(it - 1)));
		}
	}

	return true;
}

void Game::on_render()
{
	UniverseApplication::on_render();
}

void Game::on_gui()
{
}

Game game;

int entry(int argc, char** args)
{
	game.init();
	game.run();

	return 0;
}

FLAME_EXE_MAIN(entry)
