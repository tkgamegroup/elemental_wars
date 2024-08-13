#include <flame/universe/application.h>

#include <flame/xml.h>
#include <flame/foundation/system.h>
#include <flame/graphics/canvas.h>
#include <flame/universe/components/element.h>
#include <flame/universe/components/image.h>
#include <flame/universe/components/movie.h>
#include <flame/universe/components/receiver.h>
#include <flame/universe/components/camera.h>
#include <flame/universe/components/body2d.h>
#include <flame/universe/systems/scene.h>

struct Game : UniverseApplication
{
	cCameraPtr camera = nullptr;
	graphics::CanvasPtr canvas = nullptr;

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
graphics::ImagePtr img_food = nullptr;
graphics::ImagePtr img_population = nullptr;
graphics::ImagePtr img_production = nullptr;

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
	BuildingFireCreatureMaker,
	BuildingWaterCreatureMaker,
	BuildingGrassCreatureMaker,
	BuildingSteamEngine,
	BuildingWaterWheel,
	BuildingFarm,

	BuildingTypeCount
};

struct BuildingInfo
{
	std::wstring name;
	std::wstring description;
	ElementType require_tile_type = ElementNone;
};
BuildingInfo building_infos[BuildingTypeCount];

std::vector<BuildingType> available_constructions = { BuildingSteamEngine, BuildingWaterWheel, BuildingFarm };

struct cPlayer;
struct cCity;

struct cTile : Component
{
	cElementPtr element = nullptr;
	cImagePtr image = nullptr;

	uint id;
	ElementType element_type;
	cCity* owner_city = nullptr;

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
	cBody2dPtr body2d = nullptr;
	cPlayer* player = nullptr;
	cCity* city = nullptr;
	cTile* tile = nullptr;

	bool dead = false;
	int hp = 30;
	int hp_max = 30;
	bool working = false;

	cBuilding() { type_hash = "cBuilding"_h; }
	virtual ~cBuilding() {}

	void on_init() override;
	virtual void on_show_ui(sHudPtr hud) {}
};

struct cConstruction : cBuilding
{
	BuildingType construct_building;
	int need_production;
	int production = 0;

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

	int remaining_food = 0;

	int production_next_turn = 0;
	int food_production_next_turn = 0;

	int free_population = 0;
	int free_production = 0;

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

struct cCreatureMaker : cBuilding
{
	ElementType element_type;

	cCreatureMaker() { type_hash = "cCreatureMaker"_h; }
	virtual ~cCreatureMaker() {}

	void update() override;
};

struct cSteamEngine : cBuilding
{
	cSteamEngine() { type_hash = "cSteamEngine"_h; }
	virtual ~cSteamEngine() {}

	void update() override;
};

struct cWaterWheel : cBuilding
{
	cWaterWheel() { type_hash = "cWaterWheel"_h; }
	virtual ~cWaterWheel() {}

	void update() override;
};

struct cFarm : cBuilding
{
	cFarm() { type_hash = "cFarm"_h; }
	virtual ~cFarm() {}

	void update() override;
};

struct cCharacter : Component
{
	cElementPtr element = nullptr;
	cBody2dPtr body2d = nullptr;
	cPlayer* player = nullptr;

	uint id = 0;
	cvec4 color;
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
		auto e = Entity::create();
		auto element = e->add_component<cElement>();
		element->pos = tile->element->pos;
		if (city)
		{
			element->pos -= city->element->pos;
			element->pos += city->element->ext * city->element->pivot;
		}
		element->pivot = vec2(0.5f);
		element->ext = vec2(tile_sz) * 0.6f;
		auto image = e->add_component<cImage>();
		image->image = img_building;
		auto body2d = e->add_component<cBody2d>();
		body2d->type = physics::BodyStatic;
		body2d->shape_type = physics::ShapeCircle;
		body2d->radius = element->ext.x * 0.5f;
		body2d->friction = 0.3f;
		body2d->collide_bit = 1 << id;
		switch (type)
		{
		case BuildingConstruction:
		{
			element->ext *= 0.7f;
			image->image = nullptr;
			auto movie = e->add_component<cMovie>();
			movie->images.push_back(img_hammer1->desc());
			movie->images.push_back(img_hammer2->desc());
			movie->speed = 0.25f;
			auto b = new cConstruction;
			b->element = element;
			b->body2d = body2d;
			b->city = city;
			b->hp = 0;
			e->add_component_p(b);
			city->buildings->add_child(e);

			building = b;
		}
			break;
		case BuildingCity:
		{
			auto b = new cCity;
			b->element = element;
			b->body2d = body2d;
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
			b->element = element;
			b->body2d = body2d;
			b->city = city;
			e->add_component_p(b);
			city->buildings->add_child(e);

			building = b;
		}
			break;
		case BuildingFireCreatureMaker:
		case BuildingWaterCreatureMaker:
		case BuildingGrassCreatureMaker:
		{
			auto b = new cCreatureMaker;
			b->element = element;
			b->body2d = body2d;
			b->city = city;
			switch (type)
			{
			case BuildingFireCreatureMaker: b->element_type = ElementFire; break;
			case BuildingWaterCreatureMaker: b->element_type = ElementWater; break;
			case BuildingGrassCreatureMaker: b->element_type = ElementGrass; break;
			}
			e->add_component_p(b);
			city->buildings->add_child(e);

			building = b;
		}
			break;
		case BuildingSteamEngine:
		{
			auto b = new cSteamEngine;
			b->element = element;
			b->body2d = body2d;
			b->city = city;
			e->add_component_p(b);
			city->buildings->add_child(e);

			building = b;
		}
			break;
		case BuildingWaterWheel:
		{
			auto b = new cWaterWheel;
			b->element = element;
			b->body2d = body2d;
			b->city = city;
			e->add_component_p(b);
			city->buildings->add_child(e);

			building = b;
		}
			break;
		case BuildingFarm:
		{
			auto b = new cFarm;
			b->element = element;
			b->body2d = body2d;
			b->city = city;
			e->add_component_p(b);
			city->buildings->add_child(e);

			building = b;
		}
			break;
		}
		building->player = this;
		building->tile = tile;
		return building;
	}

	cCharacter* add_character(const vec2& pos, ElementType element_type)
	{
		auto color = get_element_color(element_type);
		auto e = Entity::create();
		auto element = e->add_component<cElement>();
		element->pos = pos;
		element->ext = vec2(tile_sz * 0.5f);
		element->pivot = vec2(0.5f);
		auto image = e->add_component<cImage>();
		image->image = img_sprite;
		image->tint_col = color;
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
		c->element_type = element_type;
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
	p->id = e_players_root->children.size();
	p->color = cvec4(rgbColor(vec3((1 - p->id) * 120.f, 0.7, 0.7f)) * 255.f, 255);
	e->add_component_p(p);
	e_players_root->add_child(e);
	p->add_building(nullptr, BuildingCity, tile);
	return p;
}

void cTile::on_init()
{
	element->drawers.add([this](graphics::CanvasPtr canvas) {
		auto col = cvec4(0);
		switch (element_type)
		{
		case ElementFire: col = cvec4(255, 127, 127, 255); break;
		case ElementWater: col = cvec4(127, 127, 255, 255); break;
		case ElementGrass: col = cvec4(127, 255, 127, 255); break;
		}

		if (highlighted)
		{
			auto v = sin(fract(total_time) * pi<float>()) * 0.25f + 1.25f;
			image->set_tint_col(cvec4(min(int(col.r * v), 255), min(int(col.g * v), 255), min(int(col.b * v), 255), 255));
		}
		else
			image->set_tint_col(col);
	});
}

void draw_bar(graphics::CanvasPtr canvas, const vec2& p, float w, float h, const cvec4& col)
{
	canvas->draw_rect_filled(p, p + vec2(w, h), col);
}

void cBuilding::on_init()
{
	element->drawers.add([this](graphics::CanvasPtr canvas) {
		const auto len = 20.f;
		auto r = ((float)hp / (float)hp_max);
		draw_bar(canvas, element->global_pos() - vec2(len * 0.5f, 5.f), r * len, 2, player->color);
	});
}

void cConstruction::on_init()
{
	element->drawers.add([this](graphics::CanvasPtr canvas) {
		const auto len = 20.f;
		auto r = ((float)production / (float)need_production);
		draw_bar(canvas, element->global_pos() - vec2(len * 0.5f, 3.f), r * len, 2, cvec4(255, 255, 127, 255));
	});
}

void cConstruction::update()
{
	if (city->production > 0)
	{
		auto p = need_production - production;
		p = min(p, city->production);
		city->production -= p;
		production += p;

		if (production >= need_production)
		{
			add_event([this]() {
				player->add_building(construct_building == BuildingCity ? nullptr : city, construct_building, tile);
				entity->remove_from_parent();
				return false;
			});
		}
	}
}

void cConstruction::on_show_ui(sHudPtr hud)
{
	hud->begin_layout(HudHorizontal);
	hud->push_style_var(HudStyleVarFontSize, vec4(20.f, 0.f, 0.f, 0.f));
	hud->text(std::format(L"{}/{}{}", production, need_production, ch_icon_production));
	hud->pop_style_var(HudStyleVarFontSize);
	hud->end_layout();
}

void cCity::update()
{
	remaining_food += food_production;

	if (remaining_food >= 9000)
	{
		remaining_food = 0;
		population += 1;
	}

	production = production_next_turn;
	food_production = food_production_next_turn;
	production_next_turn = 0;
	production_next_turn += 2; // 1 from city
	food_production_next_turn = -population * 2;
	food_production_next_turn += 3; // 2 from city

	free_population = population;
	free_production = production;
}

void cElementCollector::update()
{
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

void cSteamEngine::update()
{
	working = false;
	if (city->free_population > 0)
	{
		city->free_population -= 1;
		city->production_next_turn += 2;
		working = true;
	}
}

void cWaterWheel::update()
{
	working = false;
	if (city->free_population > 0)
	{
		city->free_population -= 1;
		city->production_next_turn += 2;
		working = true;
	}
}

void cFarm::update()
{
	working = false;
	if (city->free_population > 0)
	{
		city->free_population -= 1;
		city->food_production_next_turn += 2;
		working = true;
	}
}

void cCreatureMaker::update()
{
	if (sig_round)
	{
		auto pos = element->pos;
		auto c = player->add_character(vec2(pos.x + linearRand(-5.f, +5.f), pos.y + linearRand(-5.f, +5.f)), element_type);
	}
}

void cCharacter::on_init()
{
	element->drawers.add([this](graphics::CanvasPtr canvas) {
		auto pos = (element->global_pos0() + element->global_pos1()) * 0.5f;
		auto r = element->ext.x * 0.5f;
		canvas->draw_circle(pos, r, 2.f, player->color, 0.f, (float)hp / (float)hp_max);
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
	element->ext = vec2(5.f);
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

	if (character)
	{
		if (character->player->id != bullet->player_id)
		{
			bullet->dead = true;
			character->hp -= 1;
			if (character->hp <= 0)
				character->dead = true;
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
		}
	}
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

	create("Elemental Wars", uvec2(1280, 720), WindowStyleFrame | WindowStyleResizable, false, true,
		{ {"mesh_shader"_h, 0} });

	Path::set_root(L"assets", L"assets");

	canvas = hud->canvas;

	img_tile = graphics::Image::get(L"assets/tile.png");
	img_tile_select = graphics::Image::get(L"assets/tile_select.png");
	img_building = graphics::Image::get(L"assets/building.png");
	img_hammer1 = graphics::Image::get(L"assets/hammer1.png");
	img_hammer2 = graphics::Image::get(L"assets/hammer2.png");
	img_sprite = graphics::Image::get(L"assets/sprite.png");
	img_food = graphics::Image::get(L"assets/food.png");
	img_population = graphics::Image::get(L"assets/population.png");
	img_production = graphics::Image::get(L"assets/production.png");

	canvas->register_ch_color(ch_color_white, cvec4(255, 255, 255, 255));
	canvas->register_ch_color(ch_color_black, cvec4(0, 0, 0, 255));
	canvas->register_ch_color(ch_color_yes, cvec4(72, 171, 90, 255));
	canvas->register_ch_color(ch_color_no, cvec4(191, 102, 116, 255));
	for (auto i = 0; i < ElementCount; i++)
		canvas->register_ch_color(ch_color_elements[i], get_element_color((ElementType)i));
	canvas->register_ch_size(ch_size_small, 16);
	canvas->register_ch_size(ch_size_medium, 20);
	canvas->register_ch_size(ch_size_big, 24);
	canvas->register_ch_icon(ch_icon_tile, img_tile->desc());
	canvas->register_ch_icon(ch_icon_food, img_food->desc());
	canvas->register_ch_icon(ch_icon_population, img_population->desc());
	canvas->register_ch_icon(ch_icon_production, img_production->desc());

	hud->push_style_var(HudStyleVarWindowFrame, vec4(1.f, 0.f, 0.f, 0.f));

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
	building_infos[BuildingFireCreatureMaker] = {
		.name = L"Fire Creature Maker"
	};
	building_infos[BuildingWaterCreatureMaker] = {
		.name = L"Water Creature Maker"
	};
	building_infos[BuildingGrassCreatureMaker] = {
		.name = L"Grass Creature Maker"
	};
	building_infos[BuildingSteamEngine] = {
		.name = L"Steam Engine",
		.description = std::format(L"Provide Production\n+2{}", ch_icon_production),
		.require_tile_type = ElementFire
	};
	building_infos[BuildingWaterWheel] = {
		.name = L"Water Wheel",
		.description = std::format(L"Provide Production\n+2{}", ch_icon_production),
		.require_tile_type = ElementWater
	};
	building_infos[BuildingFarm] = {
		.name = L"Farm",
		.description = std::format(L"Provide Food\n+2{}", ch_icon_food),
		.require_tile_type = ElementGrass
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
			auto image = e->add_component<cImage>();
			image->image = img_tile;
			auto tile = new cTile;
			tile->element = element;
			tile->image = image;
			tile->id = id;
			e->add_component_p(tile);
			switch (linearRand(0, 2))
			{
			case 0: tile->element_type = ElementFire; break;
			case 1: tile->element_type = ElementWater; break;
			case 2: tile->element_type = ElementGrass; break;
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
		auto e_layer = Entity::create();
		auto element = e_layer->add_component<cElement>();
		element->drawers.add([this](graphics::CanvasPtr canvas) {
			for (auto& p : e_players_root->children)
			{
				auto player = p->get_component<cPlayer>();
				canvas->path = player->border_lines;
				canvas->stroke(4.f, cvec4(255), false);
				canvas->path = player->border_lines;
				canvas->stroke(2.f, player->color, false);
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

	renderer->add_render_target(RenderMode2D, camera, main_window, {}, graphics::ImageLayoutPresent);
}

bool Game::on_update()
{
	UniverseApplication::on_update();

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

		hud->begin("selecting_tile"_h, vec2(screen_size), vec2(0.f), vec2(min(0.2f, total_time - select_tile_time) * 5.f, 1.f));

		auto city = selecting_tile->owner_city;
		if (city && city->player == main_player)
		{
			if (city->tile == selecting_tile)
			{
				hud->begin_layout(HudHorizontal);

				hud->begin_layout(HudVertical);
				hud->push_style_var(HudStyleVarFontSize, vec4(20.f, 0.f, 0.f, 0.f));

				hud->push_style_color(HudStyleColorText, cvec4(0, 0, 0, 255));
				hud->progress_bar(vec2(200.f, 24.f), (float)city->hp / (float)city->hp_max,
					cvec4(127, 255, 127, 255), cvec4(127, 127, 127, 255), std::format(L"{}/{}", city->hp, city->hp_max));
				hud->pop_style_color(HudStyleColorText);

				hud->begin_layout(HudHorizontal);
				hud->text(std::format(L"{}{}", city->population, ch_icon_population));
				hud->text(std::format(L"{}{}", city->food_production, ch_icon_food));
				hud->text(std::format(L"{}{}", city->production, ch_icon_production));
				hud->end_layout();

				hud->begin_layout(HudHorizontal);
				hud->text(std::format(L"Next Citizen{}", ch_icon_population));
				hud->push_style_color(HudStyleColorText, cvec4(0, 0, 0, 255));
				hud->progress_bar(vec2(100.f, 24.f), (float)city->remaining_food / (float)9000,
					cvec4(255, 200, 127, 255), cvec4(127, 127, 127, 255), std::format(L"{}/{}", city->remaining_food, 9000));
				hud->pop_style_color(HudStyleColorText);
				hud->image(vec2(20.f), img_food->desc());
				hud->end_layout();

				hud->pop_style_var(HudStyleVarFontSize);
				hud->end_layout();

				hud->begin_layout(HudVertical);
				hud->text(L"Select a production:");
				if (hud->button(L"New City"))
				{
					auto cands = get_nearby_tiles(city->tile, 3);
					begin_select_tile([city, &cands](cTile* tile) {
						if (city->has_territory(tile))
							return false;
						for (auto c : cands)
						{
							if (c == tile)
								return true;
						}
						return false;
					}, [city](cTile* tile) {
						auto construction = (cConstruction*)main_player->add_building(city, BuildingConstruction, tile);
						construction->construct_building = BuildingCity;
						construction->need_production = 1200;
					});
				}
				hud->end_layout();

				hud->end_layout();
			}
			else
			{
				if (auto building = city->get_building(selecting_tile); building)
				{
					hud->begin_layout(HudHorizontal);
					hud->text(std::format(L"{}/{}", building->hp, building->hp_max));
					hud->end_layout();
					building->on_show_ui(hud);
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

					hud->text(L"Select a construction:");
					for (auto type : available_constructions)
					{
						auto& info = building_infos[type];
						auto ok = true;
						if (info.require_tile_type != ElementNone && info.require_tile_type != selecting_tile->element_type)
							ok = false;
						if (!ok)
							hud->push_enable(false);
						if (hud->button(info.name))
						{
							auto construction = (cConstruction*)main_player->add_building(city, BuildingConstruction, selecting_tile);
							construction->construct_building = type;
							construction->need_production = 1200;
						}
						if (!ok)
							hud->pop_enable();
						if (hud->item_hovered())
						{
							popup_str = std::format(L"{}{}{}\n"
								L"{}{}{}", 
								ch_size_big, info.name, ch_size_end,
								ch_size_medium, info.description, ch_size_end);
							if (!ok)
								popup_str += std::format(L"\n{}Can Only Build On {} Tile{}", ch_color_no, get_element_name(info.require_tile_type), ch_color_end);
						}
					}
					hud->end_layout();

					hud->end_layout();
				}
			}
		}

		hud->end();

		if (!popup_str.empty())
		{
			hud->begin_popup();
			hud->text(popup_str);
			hud->end();
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

Game game;

int entry(int argc, char** args)
{
	game.init();
	game.run();

	return 0;
}

FLAME_EXE_MAIN(entry)
