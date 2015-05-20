-- json2rfsm:
-- Parse and transform a rfsm model to a rfsm runnable instance
-- Copyright (C) 2015  Enea Scioni <enea.scioni@unife.it>
--                                 <enea.scioni@kuleuven.be>
-- License BSD as follow 
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
-- 1.   Redistributions of source code must retain the above copyright notice, 
--      this list of conditions and the following disclaimer.
--
-- 2.   Redistributions in binary form must reproduce the above copyright notice,
--      this list of conditions and the following disclaimer in the documentation
--      and/or other materials provided with the distribution.
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

utils = require("utils")
rfsm  = require("rfsm")
ss = require("solve-schema")

-- APP DEPENDENT
if not name then name = "..." end -- overload, if not existing
sf = require("state_functions")
-- END APP DEPENDENT

function selector(str)
  if type(str) ~= 'string' then return sf['default'] end
    local fnc = sf[str]
    if fnc then return fnc end
    return sf['default']
end

-- n is a state
function gen_fsm_node(n) 
  local node = {}
  
  local function _gen_transitions(subn,t)
    local s = subn
    for i,v in ipairs(t) do
      if v.events and v.events == '' then v.events = {} end
      s[#s+1] = rfsm.transition{ tgt=v.tgt, src=v.src, events=v.events }
    end
    return s
  end
  
  local function _gen_state(sub)
    local subnode = {}
    if not sub then return subnode end
    for i,v in ipairs(sub) do
      if v.type == "state" then
        if v.transitions and v.containers then --composite
          local sint = {}
          sint =  rfsm.state(_gen_state(v.containers))
          sint = _gen_transitions(sint,v.transitions)
          if v.entry then sint.entry = selector(v.entry) end
          if v.exit then  sint.exit = selector(v.exit) end
          subnode[v.id] = sint
        else  --simple node
          subnode[v.id] = rfsm.state{ entry=selector(v.entry) } 
      end
      elseif v.type == "connector" then subnode[v.id] = rfsm.conn{} end
    end
    return subnode
  end
  
  node = _gen_state(n.containers)
  node = _gen_transitions(node,n.transitions)
--   node.dbg=true
  if n.entry then node.entry=selector(n.entry) end
  if n.exit then node.entry=selector(n.exit) end
  node = rfsm.state(node)
  return node
end

-- APP DEPENDENT
if not fsm_uri then fsm_uri = "file:///home/jphilips/workspace/pnp-reference/lcsm.json" end
fsm_model = ss.expand_ref(fsm_uri)
-- END APP DEPENDENT

-- Check with metamodel TODO

a = gen_fsm_node(fsm_model.rfsm)
fsm=rfsm.init(a)

-- testing
-- rfsm.run(fsm)
-- rfsm.send_events(fsm,'e_activate')
-- print("second round")
-- rfsm.run(fsm)
-- for i,v in pairs(a) do print(i,v) end
