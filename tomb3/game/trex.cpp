#include "../tomb3/pch.h"
#include "trex.h"
#include "box.h"
#include "objects.h"
#include "collide.h"
#include "lara.h"
#include "../specific/game.h"
#include "effects.h"
#include "../3dsystem/phd_math.h"
#include "draw.h"
#include "control.h"
#include "setup.h"
#include "../specific/draweffects.h"
#include "londboss.h"
#include "items.h"
#include "lot.h"
#include "effect2.h"
#include "sound.h"
#include "pickup.h"
#include "../3dsystem/3d_gen.h"
#include "sphere.h"

static BITE_INFO trex_hit = { 0, 32, 64, 13 };
static long death_radii[5];
static long death_heights[5];
static long dradii[5] = { 1600, 5600, 6400, 5600, 1600 };
static long dheights1[5] = { -7680, -4224, -768, 2688, 6144 };
static long dheights2[5] = { -1536, -1152, -768, -384, 0 };

static void TriggerTrexBolt(PHD_VECTOR* pos, ITEM_INFO* item, long type, short ang)
{
	ITEM_INFO* bolt;
	short item_number;
	short angles[2];

	item_number = CreateItem();

	if (item_number == NO_ITEM)
		return;

	bolt = &items[item_number];
	bolt->object_number = EXTRAFX4;
	bolt->room_number = item->room_number;
	bolt->pos.x_pos = pos->x;
	bolt->pos.y_pos = pos->y;
	bolt->pos.z_pos = pos->z;
	InitialiseItem(item_number);

	if (type == 2)
	{
		bolt->pos.y_pos += item->pos.y_pos;
		bolt->pos.x_rot = short(-pos->y << 5);
		bolt->pos.y_rot = short(GetRandomControl() << 1);
	}
	else
	{
		phd_GetVectorAngles(lara_item->pos.x_pos - pos->x, lara_item->pos.y_pos - pos->y, lara_item->pos.z_pos - pos->z, angles);
		bolt->pos.x_rot = angles[1];
		bolt->pos.y_rot = ang;
		bolt->pos.z_rot = 0;
	}

	if (type == 1)
	{
		bolt->speed = 24;
		bolt->item_flags[0] = 31;
		bolt->item_flags[1] = 16;
	}
	else
	{
		bolt->speed = 16;
		bolt->item_flags[0] = -24;
		bolt->item_flags[1] = 4;

		if (type == 2)
			bolt->item_flags[2] = 1;
	}

	AddActiveItem(item_number);
}

static void ExplodeLondonBoss(ITEM_INFO* item)
{
	SHIELD_POINTS* p;
	long x, y, z, lp, lp2, rad, angle, r, g, b;

	if (bossdata.explode_count == 1 || bossdata.explode_count == 15 || bossdata.explode_count == 25 ||
		bossdata.explode_count == 35 || bossdata.explode_count == 45 || bossdata.explode_count == 55)
	{
		x = (GetRandomDraw() & 0x3FF) + item->pos.x_pos - 512;
		y = item->pos.y_pos - (GetRandomDraw() & 0x3FF) - 256;
		z = (GetRandomDraw() & 0x3FF) + item->pos.z_pos - 512;
		ExpRings[bossdata.ring_count].x = x;
		ExpRings[bossdata.ring_count].y = y;
		ExpRings[bossdata.ring_count].z = z;
		ExpRings[bossdata.ring_count].on = 3;
		bossdata.ring_count++;
		TriggerExplosionSparks(x, y, z, 3, -2, 2, 0);

		for (lp = 0; lp < 2; lp++)
			TriggerExplosionSparks(x, y, z, 3, -1, 2, 0);

		SoundEffect(SFX_BLAST_CIRCLE, &item->pos, 0x800000 | SFX_SETPITCH);
	}

	for (lp = 0; lp < 5; lp++)
	{
		if (bossdata.explode_count < 128)
		{
			death_radii[lp] = (dradii[lp] >> 4) + ((bossdata.explode_count * dradii[lp]) >> 7);
			death_heights[lp] = dheights2[lp] + ((bossdata.explode_count * (dheights1[lp] - dheights2[lp])) >> 7);
		}
	}

	p = LondonBossShield;

	for (lp = 0; lp < 5; lp++)
	{
		y = death_heights[lp];
		rad = death_radii[lp];
		angle = (wibble & 0x3F) << 3;

		for (lp2 = 0; lp2 < 8; lp2++, p++)
		{
			p->x = short((rad * rcossin_tbl[angle << 1]) >> 11);
			p->y = (short)y;
			p->z = short((rad * rcossin_tbl[(angle << 1) + 1]) >> 11);

			if (!lp || lp == 16 || bossdata.explode_count >= 64)
				p->rgb = 0;
			else
			{
				r = GetRandomDraw() & 0x3F;
				g = (GetRandomDraw() & 0x1F) + 224;
				b = (g >> 2) + (GetRandomDraw() & 0x3F);
				r = ((64 - bossdata.explode_count) * r) >> 6;
				g = ((64 - bossdata.explode_count) * g) >> 6;
				b = ((64 - bossdata.explode_count) * b) >> 6;
				p->rgb = (b << 16) | (g << 8) | r;
			}

			angle = (angle + 512) & 0xFFF;
		}
	}
}

static void LondonBossDie(short item_number)
{
	ITEM_INFO* item;

	item = &items[item_number];
	item->hit_points = DONT_TARGET;
	item->collidable = 0;
	KillItem(item_number);
	DisableBaddieAI(item_number);
	item->flags |= IFL_INVISIBLE;
}

void DinoControl(short item_number)
{
	ITEM_INFO* item;
	ITEM_INFO* target;
	ITEM_INFO* candidate;
	CREATURE_INFO* rex;
	AI_INFO info;
	long dist, best_dist, x, y, z, lp;
	short angle, n;
	PHD_VECTOR points;

	if (!CreatureActive(item_number))
		return;

	item = &items[item_number];
	rex = (CREATURE_INFO*)item->data;
	angle = 0;

	if (item->hit_points <= 0)
	{
		if (item->current_anim_state != DINO_DEATH)
		{
			item->anim_number = objects[item->object_number].anim_index + 10;
			item->frame_number = anims[item->anim_number].frame_base;
			item->current_anim_state = DINO_DEATH;
		}

		if (anims[item->anim_number].frame_end - item->frame_number == 1)
		{
			item->mesh_bits = 0;
			item->frame_number = anims[item->anim_number].frame_end - 1;

			if (!bossdata.explode_count)
			{
				bossdata.ring_count = 0;

				for (lp = 0; lp < 6; lp++)
				{
					ExpRings[lp].on = 0;
					ExpRings[lp].life = 32;
					ExpRings[lp].radius = 512;
					ExpRings[lp].speed = short(128 + (lp << 5));
					ExpRings[lp].xrot = ((GetRandomControl() & 0x1FF) - 256) << 4;
					ExpRings[lp].zrot = ((GetRandomControl() & 0x1FF) - 256) << 4;
				}

				if (!bossdata.dropped_icon)
				{
					BossDropIcon(item_number);
					bossdata.dropped_icon = 1;
				}
			}

			if (bossdata.explode_count < 256)
				bossdata.explode_count++;

			if (bossdata.explode_count > 128 && bossdata.ring_count == 6 && !ExpRings[5].life)
			{
				LondonBossDie(item_number);
				bossdata.dead = 1;
			}
			else
				ExplodeLondonBoss(item);

			return;
		}

		if (item->current_anim_state == DINO_STOP)
			item->goal_anim_state = DINO_DEATH;
		else
			item->goal_anim_state = DINO_STOP;
	}
	else
	{
		bossdata.dropped_icon = 0;
		bossdata.dead = 0;
		bossdata.ring_count = 0;
		bossdata.explode_count = 0;
		target = 0;
		best_dist = 0x7FFFFFFF;
		GetNearByRooms(item->pos.x_pos, item->pos.y_pos, item->pos.z_pos, 4096, 0, item->room_number);

		for (int i = 0; i < number_draw_rooms; i++)
		{
			for (n = room[draw_rooms[i]].item_number; n != NO_ITEM; n = candidate->next_item)
			{
				candidate = &items[n];

				if ((candidate->object_number == FLARE_ITEM || candidate->object_number == RAPTOR) &&
					candidate->hit_points && candidate->status == ITEM_ACTIVE)
				{
					x = (candidate->pos.x_pos - item->pos.x_pos) >> 6;
					y = (candidate->pos.y_pos - item->pos.y_pos) >> 6;
					z = (candidate->pos.z_pos - item->pos.z_pos) >> 6;
					dist = SQUARE(x) + SQUARE(y) + SQUARE(z);

					if (dist < best_dist)
					{
						target = candidate;
						best_dist = dist;
					}
				}
			}
		}

		rex->enemy = target;

		if (rex->hurt_by_lara)
			rex->enemy = lara_item;

		CreatureAIInfo(item, &info);
		GetCreatureMood(item, &info, 1);

		if (!item->item_flags[0] && !item->item_flags[1] && rex->enemy == lara_item)
			rex->mood = BORED_MOOD;

		CreatureMood(item, &info, 1);

		if (rex->mood == BORED_MOOD)
			rex->maximum_turn >>= 1;

		angle = CreatureTurn(item, rex->maximum_turn);

		if (item->touch_bits)
			lara_item->hit_points -= item->current_anim_state == DINO_RUN ? 10 : 1;

		rex->flags = rex->mood != ESCAPE_MOOD && !info.ahead && info.enemy_facing > -0x4000 && info.enemy_facing < 0x4000;

		if (!rex->flags && info.distance > 0x225510 && info.distance < 0x1000000 && info.bite)
			rex->flags = 1;

		if (lara.gun_type != LG_FLARE && (lara_item->current_anim_state == AS_STOP || lara_item->current_anim_state == AS_DUCK) &&
			lara_item->goal_anim_state == lara_item->current_anim_state && !item->hit_status)
		{
			if (item->item_flags[0] > 0)
				item->item_flags[0]--;
		}
		else
		{
			item->item_flags[0] = 120;
			item->item_flags[1] = 3;
		}

		//wc - knockback at health thresholds
		bool useKnockback = false;
		switch (item->item_flags[2])
		{
			case 0:
				if (item->hit_points <= (objects[item->object_number].hit_points * 0.6f))
					useKnockback = true;
				break;
			case 1:
				if (item->hit_points <= (objects[item->object_number].hit_points * 0.3f))
					useKnockback = true;
				break;
		}

		if (useKnockback)
		{
			x = lara_item->pos.x_pos - item->pos.x_pos;
			y = lara_item->pos.y_pos - item->pos.y_pos - 256;
			z = lara_item->pos.z_pos - item->pos.z_pos;
			long d;

			if (x > 8000 || x < -8000 || y > 1024 || y < -1024 || z > 8000 || z < -8000)
				d = 9000;
			else
				d = phd_sqrt(SQUARE(x) + SQUARE(y) + SQUARE(z));

			if (d < 1024 * 8 && lara.water_status == LARA_ABOVEWATER)
			{
				item->item_flags[2]++;
				lara_item->fallspeed = -100;
				short ang = (short)phd_atan(z, x);
				short dy = lara_item->pos.y_rot - ang;

				if (abs(dy) >= 0x4000)
				{
					lara_item->pos.y_rot = ang + 0x8000;
					lara_item->speed = -100;
				}
				else
				{
					lara_item->pos.y_rot = ang;
					lara_item->speed = 100;
				}
				lara_item->gravity_status = 1;
				lara_item->pos.x_rot = 0;
				lara_item->pos.z_rot = 0;
				lara_item->anim_number = ANIM_FALLDOWN;
				lara_item->frame_number = anims[ANIM_FALLDOWN].frame_base;
				lara_item->current_anim_state = AS_FORWARDJUMP;
				lara_item->goal_anim_state = AS_FORWARDJUMP;
				TriggerExplosionSparks(lara_item->pos.x_pos, lara_item->pos.y_pos, lara_item->pos.z_pos, 3, -2, 2, lara_item->room_number);
			}
		}

		points.x = trex_hit.x;
		points.y = trex_hit.y;
		points.z = trex_hit.z;
		GetJointAbsPosition(item, &points, trex_hit.mesh_num);

		switch (item->current_anim_state)
		{
		case DINO_STOP:

			if (item->required_anim_state)
				item->goal_anim_state = item->required_anim_state;
			else if (rex->mood == BORED_MOOD || rex->flags)
			{
				if (GetRandomControl() & 1)
					item->goal_anim_state = DINO_ROAR;
				else
					item->goal_anim_state = DINO_WALK;
			}
			else if (rex->mood == ESCAPE_MOOD)
			{
				if (lara.target != item && info.ahead && !item->hit_status)
				{
					if (GetRandomControl() & 1)
						item->goal_anim_state = DINO_ROAR;
					else
						item->goal_anim_state = DINO_STOP;
				}
				else
					item->goal_anim_state = DINO_RUN;
			}
			else if (info.distance < 0x225510 && info.bite)
			{
				if (item->item_flags[0])
					item->goal_anim_state = DINO_ATTACK2;
				else if (GetRandomControl() & 1)
				{
					if (item->item_flags[1])
						item->goal_anim_state = DINO_LONGROARSTART;
				}
				else if (item->item_flags[1])
					item->goal_anim_state = DINO_SNIFFSTART;
			}
			else
				item->goal_anim_state = DINO_RUN;

			break;

		case DINO_WALK:
			rex->maximum_turn = 364;

			if (rex->mood != BORED_MOOD || !rex->flags)
				item->goal_anim_state = DINO_STOP;
			else if (info.ahead && GetRandomControl() & 1)
			{
				item->required_anim_state = DINO_ROAR;
				item->goal_anim_state = DINO_STOP;
			}

			break;

		case DINO_RUN:
			rex->maximum_turn = 728;

			if (info.distance < 0x1900000 && info.bite)
				item->goal_anim_state = DINO_STOP;
			else if (rex->flags)
				item->goal_anim_state = DINO_STOP;
			else if (rex->mood == ESCAPE_MOOD || !info.ahead || GetRandomControl() & 1)
			{
				if (rex->mood == BORED_MOOD)
					item->goal_anim_state = DINO_STOP;
				else if (rex->mood == ESCAPE_MOOD && lara.target != item && info.ahead)
					item->goal_anim_state = DINO_STOP;
			}
			else
			{
				item->required_anim_state = DINO_ROAR;
				item->goal_anim_state = DINO_STOP;
			}

			break;

		case DINO_ROAR:
			//WC - fire laser bolts when roaring
			if (abs(info.angle) > 50)
			{
				if (abs(info.angle) < 728)
					item->pos.y_rot += info.angle;
				else if (info.angle >= 0)
					item->pos.y_rot += 728;
				else
					item->pos.y_rot -= 728;
			}

			rex->maximum_turn = 0;

			if (item->frame_number == anims[item->anim_number].frame_base + 48)
			{
				TriggerTrexBolt(&points, item, 0, item->pos.y_rot + 512);
				TriggerTrexBolt(&points, item, 0, item->pos.y_rot);
				TriggerTrexBolt(&points, item, 0, item->pos.y_rot - 512);
			}

			break;

		case DINO_ATTACK2:
			rex->maximum_turn = 364;

			if (rex->enemy == lara_item)
			{
				if (item->touch_bits & 0x3000)
				{
					CreatureEffect(item, &trex_hit, DoBloodSplat);
					lara_item->hit_points -= 100;
					lara_item->hit_status = 1;
					//wc - remove instakill and only use extra anim when health reaches 0
					if (lara_item->hit_points <= 0)
					{
						item->goal_anim_state = DINO_KILL;
						rex->maximum_turn = 0;
						CreatureKill(item, 11, DINO_KILL, EXTRA_DINOKILL);
					}
				}
			}
			else if (rex->enemy && item->frame_number == anims[item->anim_number].frame_base + 20)
			{
				x = abs(rex->enemy->pos.x_pos - ((objects[TREX].pivot_length * phd_sin(item->pos.y_rot)) >> W2V_SHIFT) - item->pos.x_pos);
				y = abs(rex->enemy->pos.y_pos - item->pos.y_pos);
				z = abs(rex->enemy->pos.z_pos - ((objects[TREX].pivot_length * phd_cos(item->pos.y_rot)) >> W2V_SHIFT) - item->pos.z_pos);

				if (x < 0x718E4 && y < 0x718E4 && z < 0x718E4)
				{
					if (rex->enemy->object_number == FLARE_ITEM)
						rex->enemy->hit_points = 0;
					else
					{
						rex->enemy->hit_points -= 50;
						rex->enemy->hit_status = 1;
						CreatureEffect(item, &trex_hit, DoBloodSplat);
					}
				}
			}

			if (!(GetRandomControl() & 3))
				item->required_anim_state = DINO_WALK;

			break;

		case DINO_KILL:
			rex->maximum_turn = 0;
			CreatureEffect(item, &trex_hit, DoBloodSplat);
			break;

		case DINO_LONGROARSTART:
			rex->maximum_turn = 0;

			if (item->item_flags[1] > 0 && item->frame_number == anims[item->anim_number].frame_base)
			{
				item->item_flags[1]--;

				if (rex->enemy && rex->enemy->object_number == FLARE_ITEM)
				{
					rex->enemy->hit_points = 0;
					item->item_flags[1]--;
				}
			}

			item->goal_anim_state = DINO_LONGROAREND;
			break;

		case DINO_SNIFFSTART:
			rex->maximum_turn = 0;

			if (item->item_flags[1] > 0 && item->frame_number == anims[item->anim_number].frame_base)
			{
				item->item_flags[1]--;

				if (rex->enemy && rex->enemy->object_number == FLARE_ITEM)
				{
					rex->enemy->hit_points = 0;
					item->item_flags[1] = 0;
				}
			}

			break;

		case DINO_SNIFFMID:
			rex->maximum_turn = 0;

			if (item->frame_number == anims[item->anim_number].frame_base)
			{
				if (GetRandomControl() & 1 && item->item_flags[1] && !item->item_flags[0])
				{
					item->goal_anim_state = DINO_SNIFFMID;

					if (item->item_flags[1] > 0)
						item->item_flags[1] --;
				}
				else
					item->goal_anim_state = DINO_SNIFFEND;
			}

			break;
		}
	}

	CreatureAnimation(item_number, angle, 0);

	if (item->hit_points > 0)
		item->collidable = 1;
	else
		item->collidable = 0;
}

void S_DrawTrexBoss(ITEM_INFO* item)
{
	DrawAnimatingItem(item);
	if (bossdata.explode_count)
	{
		DrawExplosionRings();
	}
}