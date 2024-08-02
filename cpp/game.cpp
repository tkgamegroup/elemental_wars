#include <flame/universe/application.h>

#include <flame/xml.h>
#include <flame/foundation/system.h>
#include <flame/graphics/canvas.h>
#include <flame/universe/components/element.h>
#include <flame/universe/components/image.h>
#include <flame/universe/components/receiver.h>
#include <flame/universe/components/camera.h>
#include <flame/universe/components/body2d.h>
#include <flame/universe/systems/scene.h>

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
graphics::ImagePtr img_hammer = nullptr;
graphics::ImagePtr img_sprite = nullptr;
graphics::ImagePtr img_food = nullptr;
graphics::ImagePtr img_population = nullptr;
graphics::ImagePtr img_production = nullptr;

enum ElementType
{
	ElementFire,
	ElementWater,
	ElementGrass
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

	BuildingTypeCount
};

inline const wchar_t* get_building_name(BuildingType type)
{
	switch (type)
	{
	case BuildingElementCollector:
		return L"Element Collector";
	case BuildingFireTower:
		return L"Fire Tower";
	case BuildingWaterTower:
		return L"Water Tower";
	case BuildingGrassTower:
		return L"Grass Tower";
	case BuildingFireCreatureMaker:
		return L"Fire Creature Maker";
	case BuildingWaterCreatureMaker:
		return L"Water Creature Maker";
	case BuildingGrassCreatureMaker:
		return L"Grass Creature Maker";
	}
	return L"";
}

struct cPlayer;
struct cCity;

struct cTile : Component
{
	cElementPtr element = nullptr;

	uint id;
	ElementType element_type;
	cCity* city = nullptr;

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

	cBuilding() { type_hash = "cBuilding"_h; }
	virtual ~cBuilding() {}

	void on_init() override;
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
	std::vector<vec2> border_lines;

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
			tile->city = this;
		}
	}

	void update_border_lines()
	{
		border_lines.clear();
		for (auto t : territories)
		{
			vec2 pos[6];
			auto c = t->element->pos;
			for (auto i = 0; i < 6; i++)
				pos[i] = arc_point(c, i * 60.f, tile_sz * 0.5f);
			if (!t->tile_rb || !has_territory(t->tile_rb))
				make_line_strips<2>(pos[0], pos[1], border_lines);
			if (!t->tile_b || !has_territory(t->tile_b))
				make_line_strips<2>(pos[1], pos[2], border_lines);
			if (!t->tile_lb || !has_territory(t->tile_lb))
				make_line_strips<2>(pos[2], pos[3], border_lines);
			if (!t->tile_lt || !has_territory(t->tile_lt))
				make_line_strips<2>(pos[3], pos[4], border_lines);
			if (!t->tile_t || !has_territory(t->tile_t))
				make_line_strips<2>(pos[4], pos[5], border_lines);
			if (!t->tile_rt || !has_territory(t->tile_rt))
				make_line_strips<2>(pos[5], pos[0], border_lines);
		}
	}

	cBuilding* get_building(cTile* tile)
	{
		for (auto& b : buildings->children)
		{
			auto building = b->get_component<cBuilding>();
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

	cPlayer() { type_hash = "cPlayer"_h; }
	virtual ~cPlayer() {}

	void on_active() override
	{
		cities = Entity::create();
		cities->name = "cities";
		cities->add_component<cElement>();
		entity->add_child(cities);
	}

	cBuilding* add_building(BuildingType type, cTile* tile)
	{
		cBuilding* building = nullptr;
		auto city = tile->city;
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
			image->image = img_hammer;
			auto b = new cConstruction;
			b->element = element;
			b->body2d = body2d;
			b->city = tile->city;
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
			b->update_border_lines();
			cities->add_child(e);

			building = b;
		}
			break;
		case BuildingElementCollector:
		{
			auto b = new cElementCollector;
			b->element = element;
			b->body2d = body2d;
			b->city = tile->city;
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
			b->city = tile->city;
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
	p->add_building(BuildingCity, tile);
	return p;
}

void cBuilding::on_init()
{
	element->drawers.add([this](graphics::CanvasPtr canvas) {
		auto pos = element->global_pos();
		auto len = ((float)hp / (float)hp_max) * 20.f;
		canvas->draw_rect_filled(pos + vec2(-len * 0.5f, -5.f), pos + vec2(len * 0.5f, -3.f), player->color);
	});
}

void cConstruction::on_init()
{
	cBuilding::on_init();
	element->drawers.add([this](graphics::CanvasPtr canvas) {
		auto pos = element->global_pos();
		auto len = ((float)production / (float)need_production) * 20.f;
		canvas->draw_rect_filled(pos + vec2(-len * 0.5f, -3.f), pos + vec2(len * 0.5f, -1.f), cvec4(100, 100, 100, 255));
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
				player->add_building(construct_building, tile);
				entity->remove_from_parent();
				return false;
			});
		}
		else
		{
			add_event([this]() {
				auto parent = entity->parent;
				parent->remove_child(entity, false);
				parent->add_child(entity);
				return false;
			});
		}
	}
}

void cCity::update()
{
	remaining_food += food_production;

	if (remaining_food >= 15)
	{
		remaining_food = 0;
		population += 1;
	}

	production = production_next_turn;
	food_production = food_production_next_turn;
	production_next_turn = 0;
	production_next_turn += 1; // 1 from city
	food_production_next_turn = -population * 2;
	food_production_next_turn += 2; // 2 from city

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

void Game::init()
{
	srand(time(0));

	create("Elemental Wars", uvec2(1280, 720), WindowStyleFrame | WindowStyleResizable, false, true,
		{ {"mesh_shader"_h, 0} });

	Path::set_root(L"assets", L"assets");

	img_tile = graphics::Image::get(L"assets/tile.png");
	img_tile_select = graphics::Image::get(L"assets/tile_select.png");
	img_building = graphics::Image::get(L"assets/building.png");
	img_hammer = graphics::Image::get(L"assets/hammer.png");
	img_sprite = graphics::Image::get(L"assets/sprite.png");
	img_food = graphics::Image::get(L"assets/food.png");
	img_population = graphics::Image::get(L"assets/population.png");
	img_production = graphics::Image::get(L"assets/production.png");

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
			element->pos = vec2(x * tile_sz * 0.75f, y * tile_sz_y);
			if (x % 2 == 1)
				element->pos.y += tile_sz_y * 0.5f;
			element->ext = vec2(tile_sz, tile_sz_y);
			element->pivot = vec2(0.5f);
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
					selecting_tile = tile;
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
				for (auto& c : player->cities->children)
				{
					auto city = c->get_component<cCity>();
					canvas->path = city->border_lines;
					canvas->stroke(2.f, player->color, false);
				}
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

		hud->begin("round"_h, vec2(screen_size.x * 0.5f, 0.f), vec2(0.f), cvec4(0, 0, 0, 255));
		hud->text(std::format(L"{}", (int)round_timer));
		hud->end();

		if (selecting_tile)
		{
			auto bottom_pannel_height = 50.f;
			hud->begin("bottom_right"_h, vec2(screen_size), vec2(0.f), cvec4(0, 0, 0, 255), vec2(1.f));
			tile_select->entity->set_enable(true);
			tile_select->set_pos(selecting_tile->element->pos);

			auto city = selecting_tile->city;
			if (city && city->player == main_player)
			{
				if (city->tile == selecting_tile)
				{
					hud->begin_layout(HudHorizontal);
					hud->image(vec2(20.f), img_population);
					hud->text(std::format(L"{}", city->population));
					hud->image(vec2(20.f), img_food);
					hud->text(std::format(L"{}", city->food_production));
					hud->image(vec2(20.f), img_production);
					hud->text(std::format(L"{}", city->production));
					hud->end_layout();
				}
				else
				{
					if (auto building = city->get_building(selecting_tile); building)
					{

					}
					else
					{
						for (auto i = (int)BuildingElementCollector; i < BuildingTypeCount; i++)
						{
							auto type = (BuildingType)i;
							if (hud->button(get_building_name(type)))
							{
								auto construction = (cConstruction*)main_player->add_building(BuildingConstruction, selecting_tile);
								construction->construct_building = type;
								construction->need_production = 30;
							}
						}
					}
				}
			}

			hud->end();
		}
		else
			tile_select->entity->set_enable(false);
	}

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

	UniverseApplication::on_update();

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

	if (input->mbtn[Mouse_Right])
	{
		camera->element->add_pos(-input->mdisp);

		if (selecting_tile)
			selecting_tile = nullptr;
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
