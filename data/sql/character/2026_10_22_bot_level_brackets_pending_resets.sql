-- Persist pending bot level resets across server restarts for Turtle WoW (vMaNGOS).
-- Without this table, a server crash/restart loses all pending resets and bots
-- keep their old levels until the next distribution cycle re-enqueues them.

CREATE TABLE IF NOT EXISTS `bot_level_brackets_pending_resets` (
  `bot_guid`     int(10) unsigned NOT NULL COMMENT 'Low GUID of the bot player',
  `target_range` tinyint(3) unsigned NOT NULL COMMENT 'Index into the level-range array',
  `is_alliance`  tinyint(1) NOT NULL DEFAULT '0' COMMENT '1 = Alliance, 0 = Horde',
  `enqueued_at`  int(10) unsigned NOT NULL COMMENT 'Unix timestamp when the reset was enqueued',
  PRIMARY KEY (`bot_guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Pending bot level resets, restored on server startup';
