#include <flame/universe/application.h>

#include <flame/xml.h>
#include <flame/foundation/system.h>
#include <flame/graphics/canvas.h>
#include <flame/audio/buffer.h>
#include <flame/audio/source.h>
#include <flame/universe/components/element.h>
#include <flame/universe/components/image.h>
#include <flame/universe/components/polygon.h>
#include <flame/universe/components/movie.h>
#include <flame/universe/components/receiver.h>
#include <flame/universe/components/camera.h>
#include <flame/universe/components/body2d.h>
#include <flame/universe/systems/scene.h>

struct Game : UniverseApplication
{
	cCameraPtr camera = nullptr;
	graphics::CanvasPtr ui_canvas = nullptr;

	void init();
	bool on_update() override;
};

const auto tile_cx = 60U;
const auto tile_cy = 30U;
const auto tile_sz = 32.f;
const auto tile_sz_y = tile_sz * 0.5f * 1.7320508071569;

graphics::ImagePtr img_tile = nullptr;
graphics::ImagePtr img_tile_select = nullptr;
graphics::ImagePtr img_building = nullptr;
graphics::ImagePtr img_hammer1 = nullptr;
graphics::ImagePtr img_hammer2 = nullptr;
graphics::ImagePtr img_sprite = nullptr;
graphics::ImagePtr img_frame = nullptr;
graphics::ImageDesc img_frame_desc;
graphics::ImagePtr img_frame2 = nullptr;
graphics::ImageDesc img_frame2_desc;
graphics::ImagePtr img_button = nullptr;
graphics::ImageDesc img_button_desc;

graphics::ImagePtr img_food = nullptr;
graphics::ImagePtr img_population = nullptr;
graphics::ImagePtr img_production = nullptr;
graphics::ImageAtlasPtr atlas_tiles = nullptr;
graphics::ImageDesc img_fire_tile = {};
graphics::ImageDesc img_water_tile = {};
graphics::ImageDesc img_grass_tile = {};

audio::SourcePtr sound_hover = nullptr;
audio::SourcePtr sound_clicked = nullptr;
audio::SourcePtr sound_construction_begin = nullptr;
audio::SourcePtr sound_construction_end = nullptr;
audio::SourcePtr sound_shot = nullptr;
audio::SourcePtr sound_hit = nullptr;

inline audio::SourcePtr load_sound_effect(const std::filesystem::path& path, float volumn)
{
	auto buf = audio::Buffer::get(path);
	auto ret = audio::Source::create();
	ret->add_buffer(buf);
	ret->set_volumn(volumn);
	ret->auto_replay = true;
	return ret;
}

enum ElementType
{
	ElementNone = -1,
	ElementFire,
	ElementWater,
	ElementGrass,
	ElementCount
};

cvec4 get_element_color(ElementType type)
{
	switch (type)
	{
	case ElementFire:
		return cvec4(255, 127, 127, 255);
	case ElementWater:
		return cvec4(127, 127, 255, 255);
	case ElementGrass:
		return cvec4(127, 255, 127, 255);
	}
	return cvec4(0);
}

const wchar_t* get_element_name(ElementType type)
{
	switch (type)
	{
	case ElementFire:
		return L"Fire";
	case ElementWater:
		return L"Water";
	case ElementGrass:
		return L"Grass";
	}
	return L"";
}

float element_effectiveness[ElementCount][ElementCount]; // attacker, defender

const wchar_t ch_color_white = graphics::CH_COLOR_BEGIN + 0;
const wchar_t ch_color_black = graphics::CH_COLOR_BEGIN + 1;
const wchar_t ch_color_yes = graphics::CH_COLOR_BEGIN + 2;
const wchar_t ch_color_no = graphics::CH_COLOR_BEGIN + 3;
const wchar_t ch_color_elements[ElementCount] = { graphics::CH_COLOR_BEGIN + 5, graphics::CH_COLOR_BEGIN + 6, graphics::CH_COLOR_BEGIN + 7 };
const wchar_t ch_color_end = graphics::CH_COLOR_END;
const wchar_t ch_size_small = graphics::CH_SIZE_BEGIN + 0;
const wchar_t ch_size_medium = graphics::CH_SIZE_BEGIN + 1;
const wchar_t ch_size_big = graphics::CH_SIZE_BEGIN + 2;
const wchar_t ch_size_end = graphics::CH_SIZE_END;
const wchar_t ch_icon_tile = graphics::CH_ICON_BEGIN + 0;
const wchar_t ch_icon_food = graphics::CH_ICON_BEGIN + 1;
const wchar_t ch_icon_population = graphics::CH_ICON_BEGIN + 2;
const wchar_t ch_icon_production = graphics::CH_ICON_BEGIN + 3;

enum BuildingType
{
	BuildingConstruction,
	BuildingCity,
	BuildingElementCollector,
	BuildingFireTower,
	BuildingWaterTower,
	BuildingGrassTower,
	BuildingFireBarracks,
	BuildingWaterBarracks,
	BuildingGrassBarracks,
	BuildingSteamMachine,
	BuildingWaterWheel,
	BuildingFarm,

	BuildingTypeCount
};

struct BuildingInfo
{
	std::wstring name;
	std::wstring description;
	ElementType require_tile_type = ElementNone;
	uint need_production = 15000;
	uint hp_max = 3000;
	graphics::ImagePtr image = nullptr;
};
BuildingInfo building_infos[BuildingTypeCount];

std::vector<BuildingType> available_constructions = { BuildingSteamMachine, BuildingFireBarracks, BuildingWaterWheel, BuildingWaterBarracks, BuildingFarm, BuildingGrassBarracks };

enum CharacterType
{
	CharacterFireElemental,
	CharacterWaterElemental,
	CharacterGrassElemental,

	CharacterTypeCount
};

struct CharacterInfo
{
	std::wstring name;
	uint hp_max = 3000;
	ElementType element_type;
	graphics::ImagePtr image = nullptr;
};
CharacterInfo character_infos[CharacterTypeCount];

struct cPlayer;
struct cBuilding;
struct cCity;

struct cTile : Component
{
	cElementPtr element = nullptr;
	cPolygonPtr polygon = nullptr;

	uint id;
	ElementType element_type;
	cCity* owner_city = nullptr;
	cBuilding* building = nullptr;

	cTile* tile_lt = nullptr;
	cTile* tile_t = nullptr;
	cTile* tile_rt = nullptr;
	cTile* tile_lb = nullptr;
	cTile* tile_b = nullptr;
	cTile* tile_rb = nullptr;

	bool highlighted = false;

	cTile() { type_hash = "cTile"_h; }
	virtual ~cTile() {}

	void on_init() override;
};

std::vector<cTile*> get_nearby_tiles(cTile* tile, uint level = 1)
{
	if (level == 0)
		return {};
	std::vector<cTile*> ret;
	int start_idx = -1;
	int end_idx = 0;
	auto add_tile = [&](cTile* t) {
		for (auto& _t : ret)
		{
			if (_t == t)
				return;
		}
		ret.push_back(t);
	};
	while (level > 0)
	{
		for (auto i = start_idx; i != end_idx; i++)
		{
			auto t = i == -1 ? tile : ret[i];
			if (t->tile_lt)
				add_tile(t->tile_lt);
			if (t->tile_t)
				add_tile(t->tile_t);
			if (t->tile_rt)
				add_tile(t->tile_rt);
			if (t->tile_lb)
				add_tile(t->tile_lb);
			if (t->tile_b)
				add_tile(t->tile_b);
			if (t->tile_rb)
				add_tile(t->tile_rb);
		}
		start_idx = end_idx;
		end_idx = ret.size();
		level--;
	}
	return ret;
}

EntityPtr e_tiles_root = nullptr;
cElementPtr tile_hover = nullptr;
cElementPtr tile_select = nullptr;

struct cBuilding : Component
{
	cElementPtr element = nullptr;
	EntityPtr e_content = nullptr;
	cPlayer* player = nullptr;
	cCity* city = nullptr;
	cTile* tile = nullptr;

	BuildingType type;
	bool dead = false;
	int hp = 1;
	int hp_max = 1;
	bool building_enable = true;
	bool working = false;
	bool working_animating = false;
	bool low_priority = false;

	cBuilding() { type_hash = "cBuilding"_h; }
	virtual ~cBuilding() {}

	void on_init() override;
	void update() override;
	virtual void on_show_ui(sHudPtr hud) {}
	void set_building_enable(bool v)
	{
		if (building_enable == v)
			return;
		building_enable = v;
		if (!building_enable)
		{
			working = false;
			low_priority = true;
		}
	}
};

struct cConstruction : cBuilding
{
	BuildingType construct_building;
	int need_production;
	int production = 0;
	int production_one_sec = 0;
	int production_one_sec_accumulate = 0;
	float one_sec_timer = 0.f;

	cConstruction() { type_hash = "cConstruction"_h; }
	virtual ~cConstruction() {}

	void on_init() override;
	void update() override;
	void on_show_ui(sHudPtr hud) override;
};

struct cCity : cBuilding
{
	int population = 1;
	int production = 0;
	int food_production = 0;

	int surplus_food = 0;

	int production_next_turn = 0;
	int food_production_next_turn = 0;

	int free_population = 0;
	int free_production = 0;

	int food_to_produce_population = 0;

	std::vector<cTile*> territories;

	EntityPtr buildings = nullptr;

	cCity() { type_hash = "cCity"_h; }
	virtual ~cCity() {}

	void on_active() override
	{
		buildings = Entity::create();
		buildings->name = "buildings";
		buildings->add_component<cElement>();
		entity->add_child(buildings);

		food_to_produce_population = calc_population_growth_needed_food();
	}

	int calc_population_growth_needed_food()
	{
		auto n = population - 1;
		return (int(pow(n, 1.5f)) + 8 * n + 15) * 1000;
	}

	void update() override;

	bool has_territory(cTile* tile)
	{
		for (auto t : territories)
		{
			if (t == tile)
				return true;
		}
		return false;
	}

	void add_territory(cTile* tile)
	{
		if (!tile)
			return;
		if (!has_territory(tile))
		{
			territories.push_back(tile);
			tile->owner_city = this;
		}
	}

	cBuilding* get_building(cTile* tile)
	{
		for (auto& b : buildings->children)
		{
			auto building = b->get_base_component<cBuilding>();
			if (building->tile == tile)
				return building;
		}
		return nullptr;
	}
};

struct cElementCollector : cBuilding
{
	float timer = 0.f;

	cElementCollector() { type_hash = "cElementCollector"_h; }
	virtual ~cElementCollector() {}

	void update() override;
};

struct cFireBarracks : cBuilding
{
	cFireBarracks() { type_hash = "cFireBarracks"_h; }
	virtual ~cFireBarracks() {}

	void update() override;
};

struct cWaterBarracks : cBuilding
{
	cWaterBarracks() { type_hash = "cWaterBarracks"_h; }
	virtual ~cWaterBarracks() {}

	void update() override;
};

struct cGrassBarracks : cBuilding
{
	cGrassBarracks() { type_hash = "cGrassBarracks"_h; }
	virtual ~cGrassBarracks() {}

	void update() override;
};

struct cSteamMachine : cBuilding
{
	int provide_production = 2;

	cSteamMachine() { type_hash = "cSteamMachine"_h; }
	virtual ~cSteamMachine() {}

	void update() override;
	void on_show_ui(sHudPtr hud) override;
};

struct cWaterWheel : cBuilding
{
	int provide_production = 2;

	cWaterWheel() { type_hash = "cWaterWheel"_h; }
	virtual ~cWaterWheel() {}

	void update() override;
	void on_show_ui(sHudPtr hud) override;
};

struct cFarm : cBuilding
{
	int provide_food = 2;

	cFarm() { type_hash = "cFarm"_h; }
	virtual ~cFarm() {}

	void update() override;
	void on_show_ui(sHudPtr hud) override;
};

struct cCharacter : Component
{
	cElementPtr element = nullptr;
	cBody2dPtr body2d = nullptr;
	cPlayer* player = nullptr;

	uint id = 0;
	cvec4 color;
	BuildingType type;
	bool dead = false;
	ElementType element_type;
	int hp = 10;
	int hp_max = 10;

	float attack_interval = 1.f;
	float attack_range = 50.f;

	bool has_target = true;
	vec2 target_pos;
	float find_timer = 0.f;
	float shoot_timer = 0.f;

	cCharacter() { type_hash = "cCharacter"_h; }
	virtual ~cCharacter() {}

	void on_init() override;
	void update() override;
};

uint character_id = 1;
EntityPtr e_characters_root = nullptr;

struct cBullet : Component
{
	cElementPtr element = nullptr;
	cBody2dPtr body2d = nullptr;
	uint player_id = -1;

	uint id = 0;
	cvec4 color;
	bool dead = false;
	float ttl = 2.f;
	ElementType element_type;

	vec2 vel;

	cBullet() { type_hash = "cBullet"_h; }
	virtual ~cBullet() {}

	void update() override;
};

uint bullet_id = 1;
EntityPtr e_bullets_root = nullptr;

cBullet* create_bullet(const vec2& pos, const vec2& velocity, ElementType element_type, cPlayer* player);

struct cPlayer : Component
{
	cElementPtr element = nullptr;

	uint id;
	cvec4 color;

	uint fire_element = 0;
	uint water_element = 0;
	uint grass_element = 0;

	EntityPtr cities = nullptr;

	std::vector<vec2> border_lines;

	cPlayer() { type_hash = "cPlayer"_h; }
	virtual ~cPlayer() {}

	void on_active() override
	{
		cities = Entity::create();
		cities->name = "cities";
		cities->add_component<cElement>();
		entity->add_child(cities);
	}

	cBuilding* add_building(cCity* city, BuildingType type, cTile* tile)
	{
		cBuilding* building = nullptr;
		auto& info = building_infos[type];
		auto e = Entity::create();
		auto element = e->add_component<cElement>();
		element->pos = tile->element->pos;
		if (city)
		{
			element->pos -= city->element->pos;
			element->pos += city->element->ext * city->element->pivot;
		}
		auto e_content = Entity::create();
		auto element_content = e_content->add_component<cElement>();
		element_content->pos = vec2(0.f, tile_sz * 0.3f);
		element_content->pivot = vec2(0.5f, 1.f);
		element_content->ext = vec2(tile_sz) * 0.6f;
		e->add_child(e_content);
		auto image = e_content->add_component<cImage>();
		image->image = info.image ? info.image : img_building;
		auto body2d = e->add_component<cBody2d>();
		body2d->type = physics::BodyStatic;
		body2d->shape_type = physics::ShapeCircle;
		body2d->radius = element->ext.x * 0.5f;
		body2d->radius = 0.f;
		body2d->friction = 0.3f;
		body2d->collide_bit = 1 << id;
		switch (type)
		{
		case BuildingConstruction:
		{
			element->ext *= 0.7f;
			auto movie = e->add_component<cMovie>();
			movie->images.push_back(img_hammer1->desc());
			movie->images.push_back(img_hammer2->desc());
			movie->speed = 0.25f;
			auto b = new cConstruction;
			b->hp = 0;
			e->add_component_p(b);
			city->buildings->add_child(e);

			sound_construction_begin->play();

			building = b;
		}
			break;
		case BuildingCity:
		{
			auto b = new cCity;
			e->add_component_p(b);

			b->add_territory(tile);
			b->add_territory(tile->tile_rb);
			b->add_territory(tile->tile_b);
			b->add_territory(tile->tile_lb);
			b->add_territory(tile->tile_lt);
			b->add_territory(tile->tile_t);
			b->add_territory(tile->tile_rt);
			cities->add_child(e);
			update_border_lines();

			building = b;
		}
			break;
		case BuildingElementCollector:
		{
			auto b = new cElementCollector;
			e->add_component_p(b);
			city->buildings->add_child(e);

			building = b;
		}
			break;
		case BuildingFireBarracks:
		{
			auto b = new cFireBarracks;
			e->add_component_p(b);
			city->buildings->add_child(e);

			building = b;
		}
			break;
		case BuildingWaterBarracks:
		{
			auto b = new cWaterBarracks;
			e->add_component_p(b);
			city->buildings->add_child(e);

			building = b;
		}
			break;
		case BuildingGrassBarracks:
		{
			auto b = new cGrassBarracks;
			e->add_component_p(b);
			city->buildings->add_child(e);

			building = b;
		}
			break;
		case BuildingSteamMachine:
		{
			auto b = new cSteamMachine;
			e->add_component_p(b);
			city->buildings->add_child(e);

			building = b;
		}
			break;
		case BuildingWaterWheel:
		{
			auto b = new cWaterWheel;
			e->add_component_p(b);
			city->buildings->add_child(e);

			building = b;
		}
			break;
		case BuildingFarm:
		{
			auto b = new cFarm;
			e->add_component_p(b);
			city->buildings->add_child(e);

			building = b;
		}
			break;
		}
		building->player = this;
		building->city = city;
		building->tile = tile;
		building->type = type;
		building->hp_max = info.hp_max;
		if (building->hp > 0)
			building->hp = info.hp_max;
		tile->building = building;
		return building;
	}

	cCharacter* add_character(const vec2& pos, CharacterType type)
	{
		auto& info = character_infos[type];
		auto e = Entity::create();
		auto element = e->add_component<cElement>();
		element->pos = pos;
		element->ext = vec2(tile_sz * 0.3f);
		element->pivot = vec2(0.5f);
		auto image = e->add_component<cImage>();
		image->image = info.image ? info.image : img_sprite;
		if (!info.image)
			image->tint_col = get_element_color(info.element_type);
		auto body2d = e->add_component<cBody2d>();
		body2d->shape_type = physics::ShapeCircle;
		body2d->radius = element->ext.x * 0.5f;
		body2d->friction = 0.3f;
		body2d->collide_bit = 1 << id;
		auto c = new cCharacter;
		c->element = element;
		c->body2d = body2d;
		c->player = this;
		c->id = character_id++;
		c->color = color;
		c->element_type = info.element_type;
		c->hp_max = info.hp_max;
		c->hp = info.hp_max;
		e->add_component_p(c);
		e_characters_root->add_child(e);
		return c;
	}

	void update_border_lines()
	{
		border_lines.clear();
		for (auto& c : cities->children)
		{
			auto city = c->get_component<cCity>();
			for (auto t : city->territories)
			{
				vec2 pos[6];
				auto c = t->element->pos;
				for (auto i = 0; i < 6; i++)
					pos[i] = arc_point(c, i * 60.f, tile_sz * 0.5f);
				if (!t->tile_rb || !city->has_territory(t->tile_rb))
					make_line_strips<2>(pos[0], pos[1], border_lines);
				if (!t->tile_b || !city->has_territory(t->tile_b))
					make_line_strips<2>(pos[1], pos[2], border_lines);
				if (!t->tile_lb || !city->has_territory(t->tile_lb))
					make_line_strips<2>(pos[2], pos[3], border_lines);
				if (!t->tile_lt || !city->has_territory(t->tile_lt))
					make_line_strips<2>(pos[3], pos[4], border_lines);
				if (!t->tile_t || !city->has_territory(t->tile_t))
					make_line_strips<2>(pos[4], pos[5], border_lines);
				if (!t->tile_rt || !city->has_territory(t->tile_rt))
					make_line_strips<2>(pos[5], pos[0], border_lines);
			}
		}
	}

	bool has_territory(cTile* tile)
	{
		for (auto& c : cities->children)
		{
			if (c->get_component<cCity>()->has_territory(tile))
				return true;
		}
		return false;
	}
};

cPlayer* main_player = nullptr;
EntityPtr e_players_root = nullptr;

const auto round_time = 30.f;
float round_timer = round_time;
bool sig_round = false;
const auto turn_time = 1.f;
float turn_timer = turn_time;
bool sig_turn = false;

cPlayer* add_player(cTile* tile)
{
	auto e = Entity::create();
	auto element = e->add_component<cElement>();
	auto p = new cPlayer;
	p->element = element;
	p->id = e_players_root->children.size();
	p->color = cvec4(rgbColor(vec3((1 - p->id) * 120.f, 0.7, 0.7f)) * 255.f, 255);
	e->add_component_p(p);
	e_players_root->add_child(e);
	p->add_building(nullptr, BuildingCity, tile);
	return p;
}

void cTile::on_init()
{
	element->drawers.add([this](graphics::CanvasPtr ui_canvas) {
		if (highlighted)
		{
			auto v = clamp(sin(fract(total_time) * pi<float>()) * 0.25f + 0.25f, 0.f, 1.f);
			polygon->color = cvec4(cvec3(v * 255.f), 255);
		}
		else
			polygon->color = cvec4(255);
	});
}

void draw_bar(graphics::CanvasPtr ui_canvas, const vec2& p, float w, float h, const cvec4& col)
{
	ui_canvas->draw_rect_filled(p, p + vec2(w, h), col);
}

void cBuilding::on_init()
{
	element = entity->get_component<cElement>();
	e_content = entity->first_child();

	element->drawers.add([this](graphics::CanvasPtr ui_canvas) {
		const auto len = 20.f;
		auto r = ((float)hp / (float)hp_max);
		draw_bar(ui_canvas, element->global_pos() - vec2(len * 0.5f, 12.f), r * len, 2, player->color);
	});
}

void cBuilding::update()
{
	if (working)
	{
		if (!working_animating)
		{
			working_animating = true;
			auto tween = sTween::instance();
			auto id = tween->begin();
			tween->set_target(id, e_content);
			tween->scale_to(id, vec2(0.8f, 1.2f), 0.4f);
			tween->set_ease(id, EaseOutBounce);
			tween->scale_to(id, vec2(1.f), 0.3f);
			tween->set_ease(id, EaseOutElastic);
			tween->wait(id, 0.3f);
			tween->set_callback(id, [this]() {
				working_animating = false;
			});
			tween->end(id);
		}
	}
}

void cConstruction::on_init()
{
	cBuilding::on_init();

	element->drawers.add([this](graphics::CanvasPtr ui_canvas) {
		const auto len = 20.f;
		auto r = ((float)production / (float)need_production);
		draw_bar(ui_canvas, element->global_pos() - vec2(len * 0.5f, 10.f), r * len, 2, cvec4(255, 255, 127, 255));
	});
}

void cConstruction::update()
{
	cBuilding::update();

	if (building_enable)
	{
		if (city->free_production > 0)
		{
			auto v = need_production - production;
			v = min(v, city->free_production);
			city->free_production -= v;
			production += v;

			production_one_sec_accumulate += v;

			hp += v * hp_max / need_production;
			hp = min(hp, hp_max);

			if (production >= need_production)
			{
				add_event([this]() {
					player->add_building(construct_building == BuildingCity ? nullptr : city, construct_building, tile);
					entity->remove_from_parent();
					return false;
				});

				sound_construction_end->play();
			}

			low_priority = true;
		}
	}

	one_sec_timer += delta_time;
	if (one_sec_timer >= 1.f)
	{
		production_one_sec = production_one_sec_accumulate;
		production_one_sec_accumulate = 0;
		one_sec_timer = 0.f;
	}
}

void cConstruction::on_show_ui(sHudPtr hud)
{
	auto sec = production_one_sec > 0 ? (need_production - production) / production_one_sec : 0;
	hud->push_style_color(HudStyleColorText, cvec4(0, 0, 0, 255));
	hud->progress_bar(vec2(200.f, 24.f), (float)production / (float)need_production,
		city->player->color, cvec4(127, 127, 127, 255), std::format(
			L"{:.1f}/{}{}{}{}    {:02d}:{:02d}",
			production / 100.f, need_production / 100, ch_color_white, ch_icon_production, ch_color_end,
			sec / 60, sec % 60));
	hud->pop_style_color(HudStyleColorText);
}

void cCity::update()
{
	cBuilding::update();

	surplus_food += food_production;

	if (surplus_food >= food_to_produce_population)
	{
		surplus_food = 0;
		population += 1;
		food_to_produce_population = calc_population_growth_needed_food();
	}

	production = production_next_turn;
	food_production = food_production_next_turn;
	production_next_turn = 0;
	production_next_turn += 10; // from city
	food_production_next_turn = -population * 2;
	food_production_next_turn += 12; // from city

	free_population = population;
	free_production = production;
}

void cElementCollector::update()
{
	cBuilding::update();

	timer += delta_time;
	if (timer >= 1.f)
	{
		timer = 0.f;

		switch (tile->element_type)
		{
		case ElementFire: player->fire_element++; break;
		case ElementWater: player->water_element++; break;
		case ElementGrass: player->grass_element++; break;
		}
	}
}

void cSteamMachine::update()
{
	cBuilding::update();

	working = false;
	if (building_enable)
	{
		if (city->free_population > 0)
		{
			city->free_population -= 1;
			city->production_next_turn += 2;
			working = true;
		}
	}
}

void cSteamMachine::on_show_ui(sHudPtr hud)
{
	if (working)
		hud->text(std::format(L"+{}{}{}{}", provide_production, ch_color_white, ch_icon_production, ch_color_end));
}

void cWaterWheel::update()
{
	cBuilding::update();

	working = false;
	if (building_enable)
	{
		if (city->free_population > 0)
		{
			city->free_population -= 1;
			city->production_next_turn += 2;
			working = true;
		}
	}
}

void cWaterWheel::on_show_ui(sHudPtr hud)
{
	if (working)
		hud->text(std::format(L"+{}{}{}{}", provide_production, ch_color_white, ch_icon_production, ch_color_end));
}

void cFarm::update()
{
	cBuilding::update();

	working = false;
	if (building_enable)
	{
		if (city->free_population > 0)
		{
			city->free_population -= 1;
			city->food_production_next_turn += 2;
			working = true;
		}
	}
}

void cFarm::on_show_ui(sHudPtr hud)
{
	if (working)
		hud->text(std::format(L"+{}{}{}{}", provide_food, ch_color_white, ch_icon_food, ch_color_end));
}

void cFireBarracks::update()
{
	cBuilding::update();

	if (sig_round)
	{
		auto pos = element->global_pos();
		auto c = player->add_character(vec2(pos.x + linearRand(-5.f, +5.f), pos.y + linearRand(-5.f, +5.f)), CharacterFireElemental);
	}
}

void cWaterBarracks::update()
{
	cBuilding::update();

	if (sig_round)
	{
		auto pos = element->global_pos();
		auto c = player->add_character(vec2(pos.x + linearRand(-5.f, +5.f), pos.y + linearRand(-5.f, +5.f)), CharacterWaterElemental);
	}
}

void cGrassBarracks::update()
{
	cBuilding::update();

	if (sig_round)
	{
		auto pos = element->global_pos();
		auto c = player->add_character(vec2(pos.x + linearRand(-5.f, +5.f), pos.y + linearRand(-5.f, +5.f)), CharacterGrassElemental);
	}
}

void cCharacter::on_init()
{
	element->drawers.add([this](graphics::CanvasPtr ui_canvas) {
		const auto len = 10.f;
		auto r = ((float)hp / (float)hp_max);
		draw_bar(ui_canvas, element->global_pos() - vec2(len * 0.5f, 5.f), r * len, 2, player->color);
	});
}

void cCharacter::update()
{
	auto pos = element->pos;
	auto dist_to_tar = distance(pos, target_pos);

	if (find_timer > 0.f)
		find_timer -= delta_time;
	if (find_timer <= 0.f)
	{
		find_timer = linearRand(0.5f, 1.f);

		std::vector<std::pair<EntityPtr, float>> cands;
		sScene::instance()->query_world2d(pos - vec2(tile_sz * 2.f), pos + vec2(tile_sz * 2.f), [&](EntityPtr e) {
			auto character = e->get_component<cCharacter>();
			if (character && character->player != player)
			{
				auto dist = distance(character->element->pos, element->pos);
				cands.emplace_back(e, dist);
			}
		});
		if (cands.empty())
		{
			for (auto& p : e_players_root->children)
			{
				auto player = p->get_component<cPlayer>();
				if (player != this->player)
				{
					for (auto& c : player->cities->children)
					{
						auto dist = distance(c->get_component<cElement>()->pos, element->pos);
						cands.emplace_back(c.get(), dist);
					}
				}
			}
		}
		if (!cands.empty())
		{
			has_target = true;
			std::sort(cands.begin(), cands.end(), [](const auto& a, const auto& b) {
				return a.second < b.second;
			});
			target_pos = cands.front().first->get_component<cElement>()->pos;
		}
		else
			has_target = false;
	}
	{
		auto t = vec2(0.f);
		if (has_target)
		{
			if (dist_to_tar > attack_range)
				t = normalize(target_pos - pos) * 32.f/*max speed*/;
		}
		auto f = t - body2d->get_velocity();
		f *= body2d->mass;
		body2d->apply_force(f);
	}

	if (shoot_timer > 0.f)
		shoot_timer -= delta_time;
	if (shoot_timer <= 0.f)
	{
		if (has_target && dist_to_tar <= attack_range + 1.f)
		{
			shoot_timer = attack_interval;
			auto dir = normalize(target_pos - pos);
			create_bullet(pos + dir * body2d->radius, dir * 100.f, element_type, player);
		}
	}
}

void cBullet::update()
{
	body2d->set_velocity(vel);

	ttl -= delta_time;
	if (ttl <= 0.f)
		dead = true;
}

cBullet* create_bullet(const vec2& pos, const vec2& velocity, ElementType element_type, cPlayer* player)
{
	auto color = get_element_color(element_type);
	auto e = Entity::create();
	auto element = e->add_component<cElement>();
	element->pos = pos;
	element->ext = vec2(2.f);
	element->pivot = vec2(0.5f);
	auto image = e->add_component<cImage>();
	image->image = img_sprite;
	image->tint_col = color;
	auto body2d = e->add_component<cBody2d>();
	body2d->shape_type = physics::ShapeCircle;
	body2d->radius = element->ext.x * 0.5f;
	body2d->friction = 0.f;
	body2d->collide_bit = 1 << player->id;
	body2d->collide_mask = ~(body2d->collide_bit);
	auto b = new cBullet;
	b->element = element;
	b->body2d = body2d;
	b->player_id = player->id;
	b->id = bullet_id++;
	b->color = color;
	b->element_type = element_type;
	b->vel = velocity;
	e->add_component_p(b);
	e_bullets_root->add_child(e);

	sound_shot->play();

	return b;
}

cTile* hovering_tile = nullptr;
cTile* selecting_tile = nullptr;
float select_tile_time = 0.f;

void on_contact(EntityPtr a, EntityPtr b)
{
	cCharacter* character = nullptr;
	cBuilding* building = nullptr;
	cBullet* bullet = nullptr;
	character = a->get_component<cCharacter>();
	building = a->get_base_component<cBuilding>();
	bullet = b->get_component<cBullet>();
	if ((!character && !building) || !bullet)
	{
		character = b->get_component<cCharacter>();
		building = b->get_base_component<cBuilding>();
		bullet = a->get_component<cBullet>();
	}
	if ((!character && !building) || !bullet)
		return;

	auto hit = false;
	if (character)
	{
		if (character->player->id != bullet->player_id)
		{
			bullet->dead = true;
			character->hp -= 10 * element_effectiveness[bullet->element_type][character->element_type];
			if (character->hp <= 0)
				character->dead = true;

			hit = true;
		}
	}
	if (building)
	{
		if (building->player->id != bullet->player_id)
		{
			bullet->dead = true;
			building->hp -= 1;
			if (building->hp <= 0)
				building->dead = true;

			hit = true;
		}
	}
	if (hit)
		sound_hit->play();
}

std::function<void(cTile*)> select_tile_callback;
bool begin_select_tile(const std::function<bool(cTile*)>& candidater, const std::function<void(cTile*)>& callback)
{
	auto n = 0;
	for (auto& t : e_tiles_root->children)
	{
		auto tile = t->get_component<cTile>();
		if (candidater(tile))
		{
			tile->highlighted = true;
			n++;
		}
	}
	if (n > 0)
		select_tile_callback = callback;
	return n > 0;
}

void end_select_tile(cTile* tile)
{
	if (tile)
		select_tile_callback(tile);
	select_tile_callback = nullptr;

	for (auto& t : e_tiles_root->children)
	{
		auto tile = t->get_component<cTile>();
		tile->highlighted = false;
	}
}

void Game::init()
{
	srand(time(0));

	UniverseApplicationOptions app_options;
	app_options.graphics_debug = true;
	app_options.graphics_configs = { {"mesh_shader"_h, 0} };
	create("Elemental Wars", uvec2(1280, 720), WindowStyleFrame | WindowStyleResizable, app_options);

	Path::set_root(L"assets", L"assets");

	ui_canvas = hud->canvas;

	img_tile = graphics::Image::get(L"assets/tile.png");
	atlas_tiles = graphics::ImageAtlas::get(L"assets/tiles.png");
	img_fire_tile = atlas_tiles->get_item("fire_tile"_h);
	img_water_tile = atlas_tiles->get_item("water_tile"_h);
	img_grass_tile = atlas_tiles->get_item("grass_tile"_h);
	img_tile_select = graphics::Image::get(L"assets/tile_select.png");
	img_building = graphics::Image::get(L"assets/building.png");
	img_hammer1 = graphics::Image::get(L"assets/hammer1.png");
	img_hammer2 = graphics::Image::get(L"assets/hammer2.png");
	img_sprite = graphics::Image::get(L"assets/sprite.png");
	img_food = graphics::Image::get(L"assets/food.png");
	img_population = graphics::Image::get(L"assets/population.png");
	img_production = graphics::Image::get(L"assets/production.png");
	img_frame = graphics::Image::get(L"assets/frame.png");
	img_frame_desc = img_frame->desc_with_config();
	img_frame2 = graphics::Image::get(L"assets/frame2.png");
	img_frame2_desc = img_frame2->desc_with_config();
	img_button = graphics::Image::get(L"assets/button.png");
	img_button_desc = img_button->desc_with_config();

	auto sp3 = graphics::Sampler::get(graphics::FilterLinear, graphics::FilterLinear, true, graphics::AddressClampToEdge);

	ui_canvas->register_ch_color(ch_color_white, cvec4(255, 255, 255, 255));
	ui_canvas->register_ch_color(ch_color_black, cvec4(0, 0, 0, 255));
	ui_canvas->register_ch_color(ch_color_yes, cvec4(72, 171, 90, 255));
	ui_canvas->register_ch_color(ch_color_no, cvec4(191, 102, 116, 255));
	for (auto i = 0; i < ElementCount; i++)
		ui_canvas->register_ch_color(ch_color_elements[i], get_element_color((ElementType)i));
	ui_canvas->register_ch_size(ch_size_small, 16);
	ui_canvas->register_ch_size(ch_size_medium, 20);
	ui_canvas->register_ch_size(ch_size_big, 24);
	ui_canvas->register_ch_icon(ch_icon_tile, img_tile->desc());
	ui_canvas->register_ch_icon(ch_icon_food, img_food->desc());
	ui_canvas->register_ch_icon(ch_icon_population, img_population->desc());
	ui_canvas->register_ch_icon(ch_icon_production, img_production->desc());

	hud->push_style_var(HudStyleVarWindowFrame, vec4(1.f, 0.f, 0.f, 0.f));
	hud->push_style_sound(HudStyleSoundButtonHover, sound_hover);
	hud->push_style_sound(HudStyleSoundButtonClicked, sound_clicked);

	hud->push_style_color(HudStyleColorButton, cvec4(255, 255, 255, 255));
	hud->push_style_color(HudStyleColorButtonHovered, cvec4(200, 200, 200, 255));
	hud->push_style_color(HudStyleColorButtonActive, cvec4(220, 220, 220, 255));
	hud->push_style_color(HudStyleColorButtonDisabled, cvec4(255, 255, 255, 255));
	hud->push_style_image(HudStyleImageButton, img_button_desc);
	hud->push_style_image(HudStyleImageButtonHovered, img_button_desc);
	hud->push_style_image(HudStyleImageButtonActive, img_button_desc);
	hud->push_style_image(HudStyleImageButtonDisabled, img_button_desc);

	sound_hover = load_sound_effect(L"assets/hover.wav", 0.15f);
	sound_clicked = load_sound_effect(L"assets/clicked.wav", 0.35f);
	sound_construction_begin = load_sound_effect(L"assets/construction_begin.wav", 0.35f);
	sound_construction_end = load_sound_effect(L"assets/construction_end.wav", 0.35f);
	sound_shot = load_sound_effect(L"assets/shot.wav", 0.25f);
	sound_hit = load_sound_effect(L"assets/hit.wav", 0.3f);

	{
		auto effectiveness = element_effectiveness[ElementFire];
		effectiveness[ElementFire] = 1.f;
		effectiveness[ElementWater] = 0.5f;
		effectiveness[ElementGrass] = 2.f;
	}
	{
		auto effectiveness = element_effectiveness[ElementWater];
		effectiveness[ElementFire] = 2.f;
		effectiveness[ElementWater] = 1.f;
		effectiveness[ElementGrass] = 0.5f;
	}
	{
		auto effectiveness = element_effectiveness[ElementGrass];
		effectiveness[ElementFire] = 0.5f;
		effectiveness[ElementWater] = 2.f;
		effectiveness[ElementGrass] = 1.f;
	}

	building_infos[BuildingCity] = {
		.name = L"City",
		.need_production = 1,
		.hp_max = 15000
	};
	building_infos[BuildingConstruction] = {
		.name = L"Construction"
	};
	building_infos[BuildingFireTower] = {
		.name = L"Element Collector"
	};
	building_infos[BuildingFireTower] = {
		.name = L"Fire Tower"
	};
	building_infos[BuildingWaterTower] = {
		.name = L"Water Tower"
	};
	building_infos[BuildingGrassTower] = {
		.name = L"Grass Tower"
	};
	building_infos[BuildingFireBarracks] = {
		.name = L"Fire Barracks",
		.description = std::format(L"Produce Fire Elemental"),
		.require_tile_type = ElementFire,
		.image = graphics::Image::get(L"assets/fire_barracks.png")
	};
	building_infos[BuildingWaterBarracks] = {
		.name = L"Water Barracks",
		.description = std::format(L"Produce Water Elemental"),
		.require_tile_type = ElementWater,
		.image = graphics::Image::get(L"assets/water_barracks.png")
	};
	building_infos[BuildingGrassBarracks] = {
		.name = L"Grass Barracks",
		.description = std::format(L"Produce Grass Elemental"),
		.require_tile_type = ElementGrass,
		.image = graphics::Image::get(L"assets/grass_barracks.png")
	};
	building_infos[BuildingSteamMachine] = {
		.name = L"Steam Machine",
		.description = std::format(L"Provide Production\n+2{}{}{}", ch_color_white, ch_icon_production, ch_color_end),
		.require_tile_type = ElementFire,
		.image = graphics::Image::get(L"assets/steam_machine.png")
	};
	building_infos[BuildingWaterWheel] = {
		.name = L"Water Wheel",
		.description = std::format(L"Provide Production\n+2{}{}{}", ch_color_white, ch_icon_production, ch_color_end),
		.require_tile_type = ElementWater,
		.image = graphics::Image::get(L"assets/water_wheel.png")
	};
	building_infos[BuildingFarm] = {
		.name = L"Farm",
		.description = std::format(L"Provide Food\n+2{}{}{}", ch_color_white, ch_icon_food, ch_color_end),
		.require_tile_type = ElementGrass,
		.image = graphics::Image::get(L"assets/farm.png")
	};

	character_infos[CharacterFireElemental] = {
		.name = L"Fire Elemental",
		.element_type = ElementFire,
		.image = graphics::Image::get(L"assets/fire_elemental.png")
	};
	character_infos[CharacterWaterElemental] = {
		.name = L"Water Elemental",
		.element_type = ElementWater,
		.image = graphics::Image::get(L"assets/water_elemental.png")
	};
	character_infos[CharacterGrassElemental] = {
		.name = L"Grass Elemental",
		.element_type = ElementGrass,
		.image = graphics::Image::get(L"assets/grass_elemental.png")
	};

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

	e_tiles_root = Entity::create();
	e_tiles_root->add_component<cElement>();
	e_element_root->add_child(e_tiles_root);
	auto stage_sz = vec2(tile_cx * tile_sz * 0.75f, tile_cy * tile_sz_y);
	for (auto y = 0; y < tile_cy; y++)
	{
		for (auto x = 0; x < tile_cx; x++)
		{
			auto id = y * tile_cx + x;
			auto e = Entity::create();
			auto element = e->add_component<cElement>();
			element->pos = vec2(x * tile_sz * 0.75f, y * tile_sz_y);
			if (x % 2 == 1)
				element->pos.y += tile_sz_y * 0.5f;
			element->ext = vec2(tile_sz, tile_sz_y);
			element->pivot = vec2(0.5f);
			auto polygon = e->add_component<cPolygon>();
			polygon->image = atlas_tiles->image;
			polygon->sampler = sp3;
			auto tile = new cTile;
			tile->element = element;
			tile->polygon = polygon;
			tile->id = id;
			e->add_component_p(tile);
			vec4 uvs;
			switch (linearRand(0, 2))
			{
			case 0:
				uvs = img_fire_tile.uvs;
				tile->element_type = ElementFire; 
				break;
			case 1:
				uvs = img_water_tile.uvs;
				tile->element_type = ElementWater; 
				break;
			case 2:
				uvs = img_grass_tile.uvs;
				tile->element_type = ElementGrass; 
				break;
			}
			auto uv0 = element->pos / stage_sz;
			for (auto i = 0; i < 6; i++)
			{
				auto v = arc_point(vec2(0.f), i * 60.f, 1.f);
				//polygon->add_pt(v * tile_sz * 0.5f, mix(uvs.xy(), uvs.zw(), fract(clamp(uv0 + (v * 0.5f + 0.5f) / vec2(tile_cx, tile_cy), vec2(0.001f), vec2(0.999f)) * 2.f)));
				polygon->add_pt(v * tile_sz * 0.5f, mix(uvs.xy(), uvs.zw(), v * 0.5f + 0.5f));
			}
			auto receiver = e->add_component<cReceiver>();
			receiver->event_listeners.add([tile](uint type, const vec2& value) {
				switch (type)
				{
				case "mouse_enter"_h:
					hovering_tile = tile;
					break;
				case "mouse_leave"_h:
					if (hovering_tile == tile)
						hovering_tile = nullptr;
					break;
				case "click"_h:
					if (select_tile_callback)
						end_select_tile(tile);
					else
						selecting_tile = tile;
					select_tile_time = total_time;
					sound_hover->play();
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

	e_players_root = Entity::create();
	e_players_root->add_component<cElement>();
	e_element_root->add_child(e_players_root);

	main_player = add_player(e_tiles_root->children[int(tile_cx * 0.25f + tile_cy * 0.25f * tile_cx)]->get_component<cTile>());
	auto opponent = add_player(e_tiles_root->children[int(tile_cx * 0.5f + tile_cy * 0.5f * tile_cx)]->get_component<cTile>());
	{
		auto city = opponent->cities->first_child()->get_component<cCity>();
		auto tile = city->territories[1];
		opponent->add_building(city, (BuildingType)(BuildingFireBarracks + tile->element_type), tile);
	}

	{
		auto e_layer = Entity::create();
		auto element = e_layer->add_component<cElement>();
		element->drawers.add([this](graphics::CanvasPtr ui_canvas) {
			for (auto& p : e_players_root->children)
			{
				auto player = p->get_component<cPlayer>();
				ui_canvas->path = player->border_lines;
				ui_canvas->stroke(4.f, cvec4(255), false);
				ui_canvas->path = player->border_lines;
				ui_canvas->stroke(2.f, player->color, false);
			}
		});
		e_element_root->add_child(e_layer);

		{
			auto e = Entity::create();
			auto element = e->add_component<cElement>();
			element->ext = vec2(tile_sz, tile_sz_y);
			element->pivot = vec2(0.5f);
			auto image = e->add_component<cImage>();
			image->image = img_tile_select;
			image->tint_col = cvec4(200, 200, 200, 255);
			e_layer->add_child(e);

			e->set_enable(false);
			tile_hover = element;
		}
		{
			auto e = Entity::create();
			auto element = e->add_component<cElement>();
			element->ext = vec2(tile_sz, tile_sz_y);
			element->pivot = vec2(0.5f);
			auto image = e->add_component<cImage>();
			image->image = img_tile_select;
			image->tint_col = cvec4(255);
			e_layer->add_child(e);

			e->set_enable(false);
			tile_select = element;
		}
	}

	e_characters_root = Entity::create();
	e_characters_root->add_component<cElement>();
	e_element_root->add_child(e_characters_root);

	e_bullets_root = Entity::create();
	e_bullets_root->add_component<cElement>();
	e_element_root->add_child(e_bullets_root);

	scene->set_world2d_contact_listener(on_contact);

	auto rt = renderer->add_render_target(RenderMode2D, camera, main_window, {}, graphics::ImageLayoutPresent);
	//rt->canvas->enable_clipping = true; // slower..
}

bool Game::on_update()
{
	UniverseApplication::on_update();

	auto screen_size = ui_canvas->size;

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

	hud->begin("round"_h, vec2(screen_size.x * 0.5f, 0.f), vec2(0.f), vec2(0.5f, 0.f));
	hud->text(std::format(L"{}", (int)round_timer));
	hud->end();

	if (hovering_tile)
	{
		tile_hover->entity->set_enable(true);
		tile_hover->set_pos(hovering_tile->element->pos);
	}
	else
		tile_hover->entity->set_enable(false);

	if (selecting_tile)
	{
		std::wstring popup_str = L"";
		graphics::ImagePtr popup_img = nullptr;

		hud->push_style_image(HudStyleImageWindowBackground, img_frame2_desc);
		hud->push_style_var(HudStyleVarWindowBorder, 150.f * img_frame2_desc.border_uvs);
		hud->push_style_color(HudStyleColorWindowFrame, cvec4(0, 0, 0, 0));
		hud->push_style_color(HudStyleColorWindowBackground, cvec4(255, 255, 255, 255));
		hud->push_style_color(HudStyleColorText, cvec4(0, 0, 0, 255));
		hud->begin("selecting_tile"_h, vec2(screen_size), vec2(0.f), vec2(min(0.2f, total_time - select_tile_time) * 5.f, 1.f));

		auto owner_city = selecting_tile->owner_city;
		auto building = selecting_tile->building;
		if (building)
		{
			auto& info = building_infos[building->type];
			if (building->type == BuildingCity && owner_city->player == main_player)
			{
				hud->begin_layout(HudHorizontal);

				hud->begin_layout(HudVertical);
				hud->push_style_var(HudStyleVarFontSize, vec4(20.f, 0.f, 0.f, 0.f));

				hud->push_style_color(HudStyleColorText, cvec4(0, 0, 0, 255));
				hud->progress_bar(vec2(200.f, 24.f), (float)owner_city->hp / (float)owner_city->hp_max,
					cvec4(127, 255, 127, 255), cvec4(127, 127, 127, 255), std::format(L"{}/{}", int(owner_city->hp / 100), int(owner_city->hp_max / 100)));
				hud->pop_style_color(HudStyleColorText);

				hud->begin_layout(HudHorizontal);
				hud->text(std::format(L"{}{}{}{}", owner_city->population, ch_color_white, ch_icon_population, ch_color_end));
				if (hud->item_hovered())
				{
					popup_str = std::format(
						L"Total Population: {}\n"
						L"Unapplied  Population: {}", 
						owner_city->population,
						owner_city->free_population);
				}
				hud->text(std::format(L"{}{}{}{}", owner_city->food_production, ch_color_white, ch_icon_food, ch_color_end));
				if (hud->item_hovered())
				{
					popup_str = std::format(
						L"Food Produced: +{}\n"
						L"Food Consumption: -{}\n"
						L"Food Surplus: {}",
						owner_city->food_production + owner_city->population * 2,
						owner_city->population * 2,
						owner_city->food_production
					);
				}
				hud->text(std::format(L"{}{}{}{}", owner_city->production, ch_color_white, ch_icon_production, ch_color_end));
				if (hud->item_hovered())
				{
					popup_str = std::format(
						L"Production Produced: {}",
						owner_city->production
					);
				}
				hud->end_layout();

				hud->begin_layout(HudHorizontal);
				hud->text(std::format(L"{}{}{}Growth", ch_color_white, ch_icon_population, ch_color_end));
				auto sec = (owner_city->food_to_produce_population - owner_city->surplus_food) / (owner_city->food_production * 60);
				hud->push_style_color(HudStyleColorText, cvec4(0, 0, 0, 255));
				hud->progress_bar(vec2(130.f, 24.f), (float)owner_city->surplus_food / (float)owner_city->food_to_produce_population,
					cvec4(255, 200, 127, 255), cvec4(127, 127, 127, 255), std::format(L"{:.1f}/{}{}{}{}    {:02d}:{:02d}", 
						owner_city->surplus_food / 100.f, owner_city->food_to_produce_population / 100,
						ch_color_white, ch_icon_food, ch_color_end,
						sec / 60, sec % 60));
				hud->pop_style_color(HudStyleColorText);
				hud->end_layout();

				hud->pop_style_var(HudStyleVarFontSize);
				hud->end_layout();

				hud->begin_layout(HudVertical);
				hud->text(L"Select a production:");
				if (hud->button(L"New City"))
				{
					auto cands = get_nearby_tiles(owner_city->tile, 3);
					begin_select_tile([&cands](cTile* tile) {
						if (main_player->has_territory(tile))
							return false;
						for (auto c : cands)
						{
							if (c == tile)
								return true;
						}
						return false;
					}, [owner_city, cands](cTile* tile) {
						if (main_player->has_territory(tile))
							return;
						auto ok = false;
						for (auto c : cands)
						{
							if (c == tile)
							{
								ok = true;
								break;
							}
						}
						if (ok)
						{
							auto construction = (cConstruction*)main_player->add_building(owner_city, BuildingConstruction, tile);
							construction->construct_building = BuildingCity;
							construction->need_production = building_infos[BuildingCity].need_production;
						}
					});
				}
				hud->end_layout();

				hud->end_layout();
			}
			else
			{
				hud->push_style_color(HudStyleColorText, building->player->color);
				hud->text(info.name);
				hud->pop_style_color(HudStyleColorText);

				hud->push_style_color(HudStyleColorText, cvec4(0, 0, 0, 255));
				hud->progress_bar(vec2(200.f, 24.f), (float)building->hp / (float)building->hp_max,
					owner_city->player->color, cvec4(127, 127, 127, 255), std::format(L"{}/{}", int(building->hp / 100), int(building->hp_max / 100)));
				hud->pop_style_color(HudStyleColorText);

				if (owner_city && owner_city->player == main_player)
				{
					hud->begin_layout(HudHorizontal);
					hud->text(building->working ? L"Working" : L"Idle");
					if (building->building_enable)
					{
						if (hud->button(L"Disable"))
							building->set_building_enable(false);
					}
					else
					{
						if (hud->button(L"Enable"))
							building->set_building_enable(true);
					}
					hud->end_layout();
					building->on_show_ui(hud);
				}
			}
		}
		else
		{
			hud->begin_layout(HudHorizontal);

			hud->begin_layout(HudVertical);
			switch (selecting_tile->element_type)
			{
			case ElementFire: hud->text(std::format(L"{}{}{}Fire Tile  ", ch_color_elements[ElementFire], ch_icon_tile, ch_color_end)); break;
			case ElementWater: hud->text(std::format(L"{}{}{}Water Tile  ", ch_color_elements[ElementWater], ch_icon_tile, ch_color_end)); break;
			case ElementGrass: hud->text(std::format(L"{}{}{}Grass Tile  ", ch_color_elements[ElementGrass], ch_icon_tile, ch_color_end)); break;
			}

			if (owner_city && owner_city->player == main_player)
			{
				hud->text(L"Select a construction:");
				for (auto type : available_constructions)
				{
					auto& info = building_infos[type];
					auto ok = true;
					if (info.require_tile_type != ElementNone && info.require_tile_type != selecting_tile->element_type)
						ok = false;
					if (!ok)
						hud->push_enable(false);
					hud->push_style_var(HudStyleVarBorder, 30.f * img_button_desc.border_uvs);
					hud->push_style_color(HudStyleColorText, cvec4(255, 255, 255, 255));
					hud->push_style_color(HudStyleColorTextDisabled, cvec4(180, 180, 180, 255));
					if (hud->button(info.name, "construction"_h + (int)info.name.c_str()))
					{
						auto construction = (cConstruction*)main_player->add_building(owner_city, BuildingConstruction, selecting_tile);
						construction->construct_building = type;
						construction->need_production = info.need_production;
					}
					hud->pop_style_var(HudStyleVarBorder);
					hud->pop_style_color(HudStyleColorText);
					hud->pop_style_color(HudStyleColorTextDisabled);
					if (!ok)
						hud->pop_enable();
					if (hud->item_hovered())
					{
						popup_img = info.image;
						auto sec = info.need_production / (owner_city->production * 60);
						popup_str = std::format(
							L"{}{}{}\n"
							L"Need: {}{}{}{}    {:02d}:{:02d}\n"
							L"{}{}{}",
							ch_size_big, info.name, ch_size_end,
							info.need_production / 100, ch_color_white, ch_icon_production, ch_color_end, sec / 60, sec % 60,
							ch_size_medium, info.description, ch_size_end);
						if (!ok)
							popup_str += std::format(L"\n{}Can Only Build On {} Tile{}", ch_color_no, get_element_name(info.require_tile_type), ch_color_end);
					}
				}
			}
			hud->end_layout();

			hud->end_layout();
		}

		hud->end();
		hud->pop_style_image(HudStyleImageWindowBackground);
		hud->pop_style_var(HudStyleVarWindowBorder);
		hud->pop_style_color(HudStyleColorWindowFrame);
		hud->pop_style_color(HudStyleColorWindowBackground);
		hud->pop_style_color(HudStyleColorText);

		if (!popup_str.empty())
		{
			hud->push_style_image(HudStyleImageWindowBackground, img_frame_desc);
			hud->push_style_var(HudStyleVarWindowBorder, 100.f * img_frame_desc.border_uvs);
			hud->push_style_color(HudStyleColorWindowFrame, cvec4(0, 0, 0, 0));
			hud->push_style_color(HudStyleColorWindowBackground, cvec4(255, 255, 255, 255));
			hud->push_style_color(HudStyleColorText, cvec4(0, 0, 0, 255));
			hud->begin_popup();
			if (popup_img)
			{
				hud->begin_layout(HudHorizontal);
				hud->image(vec2(64.f), popup_img->desc());
				hud->text(popup_str);
				hud->end_layout();
			}
			else
				hud->text(popup_str);
			hud->end();
			hud->pop_style_image(HudStyleImageWindowBackground);
			hud->pop_style_var(HudStyleVarWindowBorder);
			hud->pop_style_color(HudStyleColorWindowFrame);
			hud->pop_style_color(HudStyleColorWindowBackground);
			hud->pop_style_color(HudStyleColorText);
		}

		tile_select->entity->set_enable(true);
		tile_select->set_pos(selecting_tile->element->pos);
	}
	else
		tile_select->entity->set_enable(false);

	round_timer -= delta_time;
	sig_round = false;
	if (round_timer <= 0.f)
	{
		sig_round = true;
		round_timer = round_time;
	}

	turn_timer -= delta_time;
	sig_turn = false;
	if (turn_timer <= 0.f)
	{
		sig_turn = true;
		turn_timer = turn_time;
	}

	{
		auto n = e_characters_root->children.size();
		for (auto i = 0; i < n; i++)
		{
			auto e = e_characters_root->children[i].get();
			auto c = e->get_component<cCharacter>();
			if (c->dead)
			{
				e->remove_from_parent();
				i--;
				n--;
			}
		}
	}
	{
		auto n = e_bullets_root->children.size();
		for (auto i = 0; i < n; i++)
		{
			auto e = e_bullets_root->children[i].get();
			auto b = e->get_component<cBullet>();
			if (b->dead)
			{
				e->remove_from_parent();
				i--;
				n--;
			}
		}
	}
	{
		for (auto& p : e_players_root->children)
		{
			auto player = p->get_component<cPlayer>();
			for (auto& c : player->cities->children)
			{
				auto city = c->get_component<cCity>();
				auto& buildings = city->buildings->children;
				auto n = buildings.size();
				for (auto i = 0; i < n; i++)
				{
					auto b = buildings[i]->get_base_component<cBuilding>();
					if (b->low_priority)
					{
						std::rotate(buildings.begin() + i, buildings.begin() + i + 1, buildings.end());
						b->low_priority = false;
						i--;
					}
				}
				for (auto i = 0; i < n; i++)
				{
					auto e = buildings[i].get();
					auto b = e->get_base_component<cBuilding>();
					if (b->dead)
					{
						b->tile->building = nullptr;
						e->remove_from_parent();
						i--;
						n--;
					}
				}
			}
		}
	}

	if (input->mbtn[Mouse_Middle])
		camera->element->add_pos(-input->mdisp);

	if (input->mbtn[Mouse_Right])
	{
		if (selecting_tile)
			selecting_tile = nullptr;
		if (select_tile_callback)
			end_select_tile(nullptr);
	}

	if (input->mscroll != 0)
	{
		static float scales[] = { 1.f, 1.2f, 1.4f, 1.6f, 1.8f, 2.f, 2.5f, 3.f, 3.5f, 4.f, 4.5f, 5.f };
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

Game game;

int entry(int argc, char** args)
{
	game.init();
	game.run();

	return 0;
}

FLAME_EXE_MAIN(entry)
