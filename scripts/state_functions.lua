-- This file is part of the Pick-n-Pack reference implementation
--
-- Copyright (c) 2015, Department of Mechanical Engineering, KU Leuven, Belgium.
-- All rights reserved.
--
-- You may redistribute this software and/or modify it under the terms of the
-- "Simplified BSD License" (BSD 2-Clause License).
--
-- Redistribution and use in source and binary forms, with or without modification,
-- are permitted provided that the following conditions are met:
--
-- 1.	Redistributions of source code must retain the above copyright notice, 
--    	this list of conditions and the following disclaimer.
--
-- 2. 	Redistributions in binary form must reproduce the above copyright notice,
--	this list of conditions and the following disclaimer in the documentation
--	and/or other materials provided with the distribution.
--
-- THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
-- ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
-- WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
-- IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, 
-- INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
-- NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR 
-- PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
-- WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
-- ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
-- OF SUCH DAMAGE.
--

local state_functions = {}

-- name is set by C program
local name = name

-- this function emulates the C printf function
printf = function(s,...)
 	return io.write(s:format(...))
end
-- Deploying_entry
-- sets <current_state>
function state_functions.deploying_entry()
	printf("[%s] deploying - entry\n", name)
	current_state = "deploying"
end
function state_functions.deploying_exit()
	printf("[%s] deploying - exit\n", name)
	current_state = "deploying_exit"
end
function state_functions.creating()
	printf("[%s] creating\n", name)
	current_state = "creating"
end
function state_functions.configuring_resources()
        printf("[%s] configuring_resources\n", name)
	current_state = "configuring_resources"
end
function state_functions.deleting()
        printf("[%s] deleting\n", name)
	current_state = "deleting"
end
function state_functions.active_entry()
	printf("[%s] active - entry\n", name)
	current_state = "active"
end
function state_functions.active_exit()
	printf("[%s] active - exit\n", name)
	current_state = "active_exit"
end
function state_functions.configuring_capabilities()
        printf("[%s] configuring_capabilities\n", name)
	current_state = "configuring_capabilities"
end
function state_functions.ready_entry()
	printf("[%s] ready - entry\n", name)
	current_state = "ready"
end
function state_functions.ready_exit()
	printf("[%s] ready - exit\n", name)
	current_state = "ready_exit"
end
function state_functions.running()
        printf("[%s] running\n", name)
	current_state = "running"
end
function state_functions.pausing()
        printf("[%s] pausing\n", name)
	current_state = "pausing"
end
function state_functions.red()
        printf("[%s] red\n", name)
	current_state = "red"
end
function state_functions.green()
        printf("[%s] green\n", name)
	current_state = "green"
end
function state_functions.orange()
        printf("[%s] orange\n", name)
	current_state = "orange"
end

return state_functions
