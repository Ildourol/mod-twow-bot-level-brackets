-- Persist pending bot level resets across server restarts.
-- Without this table, a server crash or restart loses all pending resets and bots
-- keep their old levels until the next distribution cycle re-enqueues them.

CREATE TABLE IF NOT EXISTS `bot_level_brackets_pending_resets` (
  `bot_guid`     bigint(20) unsigned NOT NULL COMMENT 'Raw 64-bit value of the bot player ObjectGuid',
  `target_range` tinyint(4) NOT NULL COMMENT 'Index into the level-range array',
  `is_alliance`  tinyint(1) NOT NULL DEFAULT '0' COMMENT '1 = Alliance, 0 = Horde',
  `enqueued_at`  int(10) unsigned NOT NULL COMMENT 'Unix timestamp when the reset was enqueued',
  PRIMARY KEY (`bot_guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Pending bot level resets restored on server startup';
