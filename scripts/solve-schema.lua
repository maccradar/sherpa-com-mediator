-- solve-schema:
-- Solve dereferencing in json-schema, by fetching the json-schema info
-- Copyright (C) 2015  Enea Scioni <enea.scioni@unife.it>
--                                 <enea.scioni@kuleuven.be>
-- MIT License
-- Script imported from the project uMF-schema

local url   = require("socket.url")
local http  = require("socket.http")
local https = require("ssl.https")
local json  = require("json")
local utils = require("utils")

local M = {}

local function expand_internal(schema,field)
  local lschema = schema
  local tab = utils.split(field,"%/") --field:gsub("#","",1)
  utils.foreach(function(v,k) lschema = lschema[v] end,tab)
  return lschema
end

local function fetch_schema_https(authority)
  if type(authority) ~= 'string' then return end
  local body = https.request(authority)
  return json.decode(body)
end

local function fetch_schema_http(authority)
  if type(authority) ~= 'string' then return end
  local body = http.request(authority)
  return json.decode(body)
end

local function fetch_local_schema(path)
  local fn = path
  fd = io.input(path)
  local body = fd:read("*all")
  return json.decode(body)
end

local function expand_ref(uri,schema)
  local schema = schema or {}
  if type(uri) ~= 'string' then return end
  local parsed = url.parse(uri)
  
  if parsed.scheme == nil and type(parsed.fragment) == 'string' then
    return expand_internal(schema, parsed.fragment)
  elseif parsed.scheme == 'http' then
    local obj = fetch_schema_http(parsed.scheme.."://"..parsed.authority..parsed.path)
    if obj ~= nil and parsed.fragment ~= nil then
      return expand_internal(obj,parsed.fragment)
    end
    return obj
  elseif parsed.scheme == 'https' then
    local obj = fetch_schema_https(parsed.scheme.."://"..parsed.authority..parsed.path)
    if obj ~= nil and parsed.fragment ~= nil then
      return expand_internal(obj,parsed.fragment)
    end
    return obj
  elseif parsed.scheme == 'file' then
    local obj = fetch_local_schema(parsed.authority..parsed.path)
    if obj ~= nil and parsed.fragment ~= nil then
      return expand_internal(obj,parsed.fragment)
    end
    return obj
  end
  return
end


--- Expose
M.version = "0.0.2"
M.expand_ref = expand_ref

return M