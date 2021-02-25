# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected (IGNORE_EXIT_CODES => 0, [<<'EOF']);
(swap-fork) begin
(swap-fork) end
EOF
pass;
