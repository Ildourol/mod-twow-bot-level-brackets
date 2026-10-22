-- Bot Level Brackets: register admin commands in the world command table.
-- Applied to the world database.

DELETE FROM `command` WHERE `name` IN (
    'blb',
    'blb reload',
    'blb status',
    'blb force',
    'blb pending',
    'blb cleanup',
    'blb stats',
    'botbrackets',
    'botbrackets reload',
    'botbrackets status',
    'botbrackets force',
    'botbrackets pending',
    'botbrackets guildcleanup',
    'botbrackets stats'
);

INSERT INTO `command` (`name`, `security`, `help`) VALUES
    ('blb', 3, 'Syntax: .blb <reload|status|force|pending|cleanup|stats>\r\nBot Level Brackets administration commands.'),
    ('blb reload', 3, 'Syntax: .blb reload\r\nReloads the Bot Level Brackets configuration and balances bracket allocations without server restart.'),
    ('blb status', 1, 'Syntax: .blb status\r\nDisplays live tabular report of bot allocations across all brackets for Alliance and Horde.'),
    ('blb force', 3, 'Syntax: .blb force\r\nTriggers an immediate full distribution cycle across online bots.'),
    ('blb pending', 3, 'Syntax: .blb pending\r\nDisplays current pending reset and teleport queue depths, TTL expiration settings, and age statistics.'),
    ('blb cleanup', 3, 'Syntax: .blb cleanup\r\nPurges offline-only guilds from the persistent guild tracker database.'),
    ('blb stats', 1, 'Syntax: .blb stats\r\nDisplays cumulative operational statistics (cycles run, bots adjusted, bots teleported, bots skipped).');
