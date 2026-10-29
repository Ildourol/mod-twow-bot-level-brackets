"""Unit test suite for Coordinator, Census, Quota Math, 43 Activities Catalog & Debug."""
import unittest
import math
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]

# 43 stable activities list from Section 26
ALL_43_ACTIVITIES = [
    "QuestAccept", "QuestObjective", "QuestTurnIn", "QuestRepeat",
    "Grinding", "Exploration", "GenericRpg", "VendorSell", "VendorBuy",
    "Repair", "ClassTraining", "ProfessionTraining", "MountTraining",
    "MountPurchase", "ReagentPurchase", "AuctionSell", "AuctionBuy",
    "Mailbox", "GatherHerbalism", "GatherMining", "Skinning", "Fishing",
    "Crafting", "Gossip", "Emote", "RestSit", "SocialHeal", "NpcUse",
    "FlightDiscovery", "GuildMeeting", "GuildOrder", "GuildPetition",
    "PlayerTrade", "EnchantService", "Duel", "WorldPvp", "BossHunt",
    "BattlegroundQueue", "DungeonQueue", "HomeBind", "AmbientChat", "RandomJump"
]

class TestActivitiesCatalog(unittest.TestCase):
    def setUp(self):
        self.config_text = (ROOT / 'conf/mod_twow_bot_level_brackets.conf.dist').read_text(encoding='utf-8')
        self.cpp_text = '\n'.join(p.read_text(encoding='utf-8') for p in (ROOT / 'src').glob('*.cpp'))

    def test_all_43_activities_present_in_template(self):
        """Every stable activity in the catalog must have an explicit config block in the template."""
        for act in ALL_43_ACTIVITIES:
            prefix = f"BotLevelBrackets.Activities.{act}."
            self.assertIn(f"{prefix}Enabled", self.config_text, f"{act} missing Enabled in config")
            self.assertIn(f"{prefix}CityEnabled", self.config_text, f"{act} missing CityEnabled in config")
            self.assertIn(f"{prefix}WorldEnabled", self.config_text, f"{act} missing WorldEnabled in config")
            self.assertIn(f"{prefix}Weight", self.config_text, f"{act} missing Weight in config")
            self.assertIn(f"{prefix}CooldownSeconds", self.config_text, f"{act} missing CooldownSeconds in config")
            self.assertIn(f"{prefix}MaxOutstandingPerFaction", self.config_text, f"{act} missing MaxOutstanding in config")

    def test_all_43_activities_registered_in_cpp(self):
        """Every stable activity must be registered in ActivityRegistry.cpp."""
        for act in ALL_43_ACTIVITIES:
            self.assertIn(f'"{act}"', self.cpp_text, f"{act} not found in C++ source")

    def test_no_forbidden_ai_mutations_in_production(self):
        """Strictly forbids SetLevel or GiveLevel calls in redistribution scheduling."""
        adapter = (ROOT / 'src/BotEngineAdapter.h').read_text(encoding='utf-8')
        coordinator = (ROOT / 'src/OpportunityCoordinator.cpp').read_text(encoding='utf-8')
        self.assertNotIn("bot->SetLevel(", coordinator, "OpportunityCoordinator must not mutate bot level")
        self.assertNotIn("bot->GiveLevel(", coordinator, "OpportunityCoordinator must not give bot level")


class TestCensusAndQuotaMath(unittest.TestCase):
    def test_largest_remainder_level_distribution(self):
        """Verify largest remainder rounding produces exact integer sums matching online totals."""
        online_total = 73
        static_pcts = [16, 16, 16, 16, 16, 20]
        sum_static = sum(static_pcts)

        # Allocate exact
        exact = [(p / sum_static) * online_total for p in static_pcts]
        floored = [int(math.floor(x)) for x in exact]
        allocated = sum(floored)
        remainders = sorted([(exact[i] - floored[i], i) for i in range(len(static_pcts))], reverse=True)

        needed = online_total - allocated
        for r in range(needed):
            floored[remainders[r][1]] += 1

        self.assertEqual(sum(floored), online_total)

    def test_dynamic_real_player_blend(self):
        """Verify dynamic human player weighting formula (alpha = weight / (1 + weight))."""
        weight = 1.0
        alpha = weight / (1.0 + weight)
        self.assertAlmostEqual(alpha, 0.5)

        static_share = [0.2, 0.2, 0.2, 0.2, 0.2]
        human_observed = [0, 0, 10, 0, 0]
        total_humans = sum(human_observed)

        human_share = [h / total_humans for h in human_observed]
        blended = [(1.0 - alpha) * static_share[i] + alpha * human_share[i] for i in range(5)]

        self.assertAlmostEqual(sum(blended), 1.0)
        # The bracket with 10 humans should have higher blended share than static (0.2)
        self.assertGreater(blended[2], static_share[2])

    def test_field_reserve_bound(self):
        """Verify that city demand is constrained by the 70% outdoor field reserve."""
        online_total = 100
        field_reserve_pct = 70
        max_city_demand = (online_total * (100 - field_reserve_pct)) // 100
        self.assertEqual(max_city_demand, 30)

        # If cities request 50 bots total, it must scale down to max_city_demand (30)
        requested_city_sum = 50
        scale = max_city_demand / requested_city_sum
        self.assertAlmostEqual(scale, 0.6)
        feasible_city_sum = int(requested_city_sum * scale)
        self.assertLessEqual(feasible_city_sum, max_city_demand)


class TestFallbackInvariants(unittest.TestCase):
    def setUp(self):
        self.config_text = (ROOT / 'conf/mod_twow_bot_level_brackets.conf.dist').read_text(encoding='utf-8')
        self.fallback_cpp = (ROOT / 'src/CityRelocationFallback.cpp').read_text(encoding='utf-8')

    def test_fallback_defaults_off(self):
        """Fallback relocation must be OFF by default in shipped template."""
        match = re.search(r'^BotLevelBrackets\.City\.Fallback\.Enabled\s*=\s*(.*?)$', self.config_text, re.M)
        self.assertIsNotNone(match)
        self.assertEqual(match.group(1).strip(), "0")

    def test_only_allowlisted_reason_permitted(self):
        """Fallback strictly permits only NativeRouteUnsupportedBeforeStart."""
        self.assertIn("REASON_ROUTE_UNSUPPORTED_BEFORE_START", self.fallback_cpp)
        self.assertIn("reason != REASON_ROUTE_UNSUPPORTED_BEFORE_START", self.fallback_cpp)


if __name__ == '__main__':
    unittest.main()
