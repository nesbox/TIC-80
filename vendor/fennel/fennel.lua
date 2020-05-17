--[[
Copyright (c) 2016-2019 Calvin Rose and contributors
Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
]]

-- Make global variables local.
local setmetatable = setmetatable
local getmetatable = getmetatable
local type = type
local assert = assert
local pairs = pairs
local ipairs = ipairs
local tostring = tostring
local unpack = unpack or table.unpack

--
-- Main Types and support functions
--

-- Like pairs, but gives consistent ordering every time. On 5.1, 5.2, and LuaJIT
-- pairs is already stable, but on 5.3 every run gives different ordering.
local function stablepairs(t)
    local keys, succ = {}, {}
    for k in pairs(t) do table.insert(keys, k) end
    table.sort(keys, function(a, b) return tostring(a) < tostring(b) end)
    for i,k in ipairs(keys) do succ[k] = keys[i+1] end
    local function stablenext(tbl, idx)
        if idx == nil then return keys[1], tbl[keys[1]] end
        return succ[idx], tbl[succ[idx]]
    end
    return stablenext, t, nil
end

-- Map function f over sequential table t, removing values where f returns nil.
-- Optionally takes a target table to insert the mapped values into.
local function map(t, f, out)
    out = out or {}
    if type(f) ~= "function" then local s = f f = function(x) return x[s] end end
    for _,x in ipairs(t) do
        local v = f(x)
        if v then table.insert(out, v) end
    end
    return out
end

-- Map function f over key/value table t, similar to above, but it can return a
-- sequential table if f returns a single value or a k/v table if f returns two.
-- Optionally takes a target table to insert the mapped values into.
local function kvmap(t, f, out)
    out = out or {}
    if type(f) ~= "function" then local s = f f = function(x) return x[s] end end
    for k,x in stablepairs(t) do
        local korv, v = f(k, x)
        if korv and not v then table.insert(out, korv) end
        if korv and v then out[korv] = v end
    end
    return out
end

local function allPairs(t)
    assert(type(t) == 'table', 'allPairs expects a table')
    local seen = {}
    local function allPairsNext(_, state)
        local nextState, value = next(t, state)
        if seen[nextState] then
            return allPairsNext(nil, nextState)
        elseif nextState then
            seen[nextState] = true
            return nextState, value
        end
        local meta = getmetatable(t)
        if meta and meta.__index then
            t = meta.__index
            return allPairsNext(t)
        end
    end
    return allPairsNext
end

local function deref(self) return self[1] end

local function listToString(self, tostring2)
    return '(' .. table.concat(map(self, tostring2 or tostring), ' ', 1, #self) .. ')'
end

local SYMBOL_MT = { 'SYMBOL', __tostring = deref, __fennelview = deref }
local EXPR_MT = { 'EXPR', __tostring = deref }
local VARARG = setmetatable({ '...' },
    { 'VARARG', __tostring = deref, __fennelview = deref })
local LIST_MT = { 'LIST', __tostring = listToString, __fennelview = listToString }
local SEQUENCE_MT = { 'SEQUENCE' }

-- Load code with an environment in all recent Lua versions
local function loadCode(code, environment, filename)
    environment = environment or _ENV or _G
    if setfenv and loadstring then
        local f = assert(loadstring(code, filename))
        setfenv(f, environment)
        return f
    else
        return assert(load(code, filename, "t", environment))
    end
end

-- Safely load an environment variable
local getenv = os and os.getenv or function() return nil end

local function debugOn(flag)
    local level = getenv("FENNEL_DEBUG") or ""
    return level == "all" or level:find(flag)
end

-- Create a new list. Lists are a compile-time construct in Fennel; they are
-- represented as tables with a special marker metatable. They only come from
-- the parser, and they represent code which comes from reading a paren form;
-- they are specifically not cons cells.
local function list(...)
    return setmetatable({...}, LIST_MT)
end

-- Create a new symbol. Symbols are a compile-time construct in Fennel and are
-- not exposed outside the compiler. Symbols have source data describing what
-- file, line, etc that they came from.
local function sym(str, scope, source)
    local s = {str, scope = scope}
    for k, v in pairs(source or {}) do
        if type(k) == 'string' then s[k] = v end
    end
    return setmetatable(s, SYMBOL_MT)
end

-- Create a new sequence. Sequences are tables that come from the parser when
-- it encounters a form with square brackets. They are treated as regular tables
-- except when certain macros need to look for binding forms, etc specifically.
local function sequence(...)
   return setmetatable({...}, SEQUENCE_MT)
end

-- Create a new expr
-- etype should be one of
--   "literal", -- literals like numbers, strings, nil, true, false
--   "expression", -- Complex strings of Lua code, may have side effects, etc, but is an expression
--   "statement", -- Same as expression, but is also a valid statement (function calls).
--   "vargs", -- varargs symbol
--   "sym", -- symbol reference
local function expr(strcode, etype)
    return setmetatable({ strcode, type = etype }, EXPR_MT)
end

local function varg()
    return VARARG
end

local function isVarg(x)
    return x == VARARG and x
end

-- Checks if an object is a List. Returns the object if is a List.
local function isList(x)
    return type(x) == 'table' and getmetatable(x) == LIST_MT and x
end

-- Checks if an object is a symbol. Returns the object if it is a symbol.
local function isSym(x)
    return type(x) == 'table' and getmetatable(x) == SYMBOL_MT and x
end

-- Checks if an object any kind of table, EXCEPT list or symbol
local function isTable(x)
    return type(x) == 'table' and
        x ~= VARARG and
        getmetatable(x) ~= LIST_MT and getmetatable(x) ~= SYMBOL_MT and x
end

-- Checks if an object is a sequence (created with a [] literal)
local function isSequence(x)
   return type(x) == 'table' and getmetatable(x) == SEQUENCE_MT and x
end

-- Returns a shallow copy of its table argument. Returns an empty table on nil.
local function copy(from)
   local to = {}
   for k, v in pairs(from or {}) do to[k] = v end
   return to
end

--
-- Parser
--

-- Convert a stream of chunks to a stream of bytes.
-- Also returns a second function to clear the buffer in the byte stream
local function granulate(getchunk)
    local c = ''
    local index = 1
    local done = false
    return function (parserState)
        if done then return nil end
        if index <= #c then
            local b = c:byte(index)
            index = index + 1
            return b
        else
            c = getchunk(parserState)
            if not c or c == '' then
                done = true
                return nil
            end
            index = 2
            return c:byte(1)
        end
    end, function ()
        c = ''
    end
end

-- Convert a string into a stream of bytes
local function stringStream(str)
    local index = 1
    return function()
        local r = str:byte(index)
        index = index + 1
        return r
    end
end

-- Table of delimiter bytes - (, ), [, ], {, }
-- Opener keys have closer as the value, and closers keys
-- have true as their value.
local delims = {
    [40] = 41,        -- (
    [41] = true,      -- )
    [91] = 93,        -- [
    [93] = true,      -- ]
    [123] = 125,      -- {
    [125] = true      -- }
}

local function iswhitespace(b)
    return b == 32 or (b >= 9 and b <= 13)
end

local function issymbolchar(b)
    return b > 32 and
        not delims[b] and
        b ~= 127 and -- "<BS>"
        b ~= 34 and -- "\""
        b ~= 39 and -- "'"
        b ~= 126 and -- "~"
        b ~= 59 and -- ";"
        b ~= 44 and -- ","
        b ~= 64 and -- "@"
        b ~= 96 -- "`"
end

local prefixes = { -- prefix chars substituted while reading
    [96] = 'quote', -- `
    [44] = 'unquote', -- ,
    [39] = 'quote', -- '
    [35] = 'hashfn' -- #
}

-- The resetRoot function needs to be called at every exit point of the compiler
-- including when there's a parse error or compiler error. Introduce it up here
-- so error functions have access to it, and set it when we have values below.
local resetRoot = nil

-- Parse one value given a function that
-- returns sequential bytes. Will throw an error as soon
-- as possible without getting more bytes on bad input. Returns
-- if a value was read, and then the value read. Will return nil
-- when input stream is finished.
local function parser(getbyte, filename, options)

    -- Stack of unfinished values
    local stack = {}

    -- Provide one character buffer and keep
    -- track of current line and byte index
    local line = 1
    local byteindex = 0
    local lastb
    local function ungetb(ub)
        if ub == 10 then line = line - 1 end
        byteindex = byteindex - 1
        lastb = ub
    end
    local function getb()
        local r
        if lastb then
            r, lastb = lastb, nil
        else
            r = getbyte({ stackSize = #stack })
        end
        byteindex = byteindex + 1
        if r == 10 then line = line + 1 end
        return r
    end

    -- If you add new calls to this function, please update fenneldfriend.fnl
    -- as well to add suggestions for how to fix the new error.
    local function parseError(msg)
        if resetRoot then resetRoot() end
        local override = options and options["parse-error"]
        if override then override(msg, filename or "unknown", line or "?", byteindex) end
        return error(("Parse error in %s:%s: %s"):
                format(filename or "unknown", line or "?", msg), 0)
    end

    -- Parse stream
    return function()

        -- Dispatch when we complete a value
        local done, retval
        local whitespaceSinceDispatch = true
        local function dispatch(v)
            if #stack == 0 then
                retval = v
                done = true
            elseif stack[#stack].prefix then
                local stacktop = stack[#stack]
                stack[#stack] = nil
                return dispatch(list(sym(stacktop.prefix), v))
            else
                table.insert(stack[#stack], v)
            end
            whitespaceSinceDispatch = false
        end

        -- Throw nice error when we expect more characters
        -- but reach end of stream.
        local function badend()
            local accum = map(stack, "closer")
            parseError(('expected closing delimiter%s %s'):format(
                #stack == 1 and "" or "s",
                string.char(unpack(accum))))
        end

        -- The main parse loop
        repeat
            local b

            -- Skip whitespace
            repeat
                b = getb()
                if b and iswhitespace(b) then
                    whitespaceSinceDispatch = true
                end
            until not b or not iswhitespace(b)
            if not b then
                if #stack > 0 then badend() end
                return nil
            end

            if b == 59 then -- ; Comment
                repeat
                    b = getb()
                until not b or b == 10 -- newline
            elseif type(delims[b]) == 'number' then -- Opening delimiter
                if not whitespaceSinceDispatch then
                    parseError('expected whitespace before opening delimiter '
                                   .. string.char(b))
                end
                table.insert(stack, setmetatable({
                    closer = delims[b],
                    line = line,
                    filename = filename,
                    bytestart = byteindex
                }, LIST_MT))
            elseif delims[b] then -- Closing delimiter
                if #stack == 0 then parseError('unexpected closing delimiter '
                                                   .. string.char(b)) end
                local last = stack[#stack]
                local val
                if last.closer ~= b then
                    parseError('mismatched closing delimiter ' .. string.char(b) ..
                               ', expected ' .. string.char(last.closer))
                end
                last.byteend = byteindex -- Set closing byte index
                if b == 41 then -- ; )
                    val = last
                elseif b == 93 then -- ; ]
                    val = sequence()
                    for i = 1, #last do
                        val[i] = last[i]
                    end
                    -- for table literals we can store file/line/offset source
                    -- data in fields on the table itself, because the AST node
                    -- *is* the table, and the fields would show up in the
                    -- compiled output. keep them on the metatable instead.
                    setmetatable(val, last)
                else -- ; }
                    if #last % 2 ~= 0 then
                        byteindex = byteindex - 1
                        parseError('expected even number of values in table literal')
                    end
                    val = {}
                    setmetatable(val, last) -- see note above about source data
                    for i = 1, #last, 2 do
                        if(tostring(last[i]) == ":" and isSym(last[i + 1])
                           and isSym(last[i])) then
                            last[i] = tostring(last[i + 1])
                        end
                        val[last[i]] = last[i + 1]
                    end
                end
                stack[#stack] = nil
                dispatch(val)
            elseif b == 34 then -- Quoted string
                local state = "base"
                local chars = {34}
                stack[#stack + 1] = {closer = 34}
                repeat
                    b = getb()
                    chars[#chars + 1] = b
                    if state == "base" then
                        if b == 92 then
                            state = "backslash"
                        elseif b == 34 then
                            state = "done"
                        end
                    else
                        -- state == "backslash"
                        state = "base"
                    end
                until not b or (state == "done")
                if not b then badend() end
                stack[#stack] = nil
                local raw = string.char(unpack(chars))
                local formatted = raw:gsub("[\1-\31]", function (c) return '\\' .. c:byte() end)
                local loadFn = loadCode(('return %s'):format(formatted), nil, filename)
                dispatch(loadFn())
            elseif prefixes[b] then
                -- expand prefix byte into wrapping form eg. '`a' into '(quote a)'
                table.insert(stack, {
                    prefix = prefixes[b]
                })
                local nextb = getb()
                if iswhitespace(nextb) then
                    if b == 35 then
                        stack[#stack] = nil
                        dispatch(sym('#'))
                    else
                        parseError('invalid whitespace after quoting prefix')
                    end
                end
                ungetb(nextb)
            elseif issymbolchar(b) or b == string.byte("~") then -- Try symbol
                local chars = {}
                local bytestart = byteindex
                repeat
                    chars[#chars + 1] = b
                    b = getb()
                until not b or not issymbolchar(b)
                if b then ungetb(b) end
                local rawstr = string.char(unpack(chars))
                if rawstr == 'true' then dispatch(true)
                elseif rawstr == 'false' then dispatch(false)
                elseif rawstr == '...' then dispatch(VARARG)
                elseif rawstr:match('^:.+$') then -- colon style strings
                    dispatch(rawstr:sub(2))
                elseif rawstr:match("^~") and rawstr ~= "~=" then
                    -- for backwards-compatibility, special-case allowance of ~=
                    -- but all other uses of ~ are disallowed
                    parseError("illegal character: ~")
                else
                    local forceNumber = rawstr:match('^%d')
                    local numberWithStrippedUnderscores = rawstr:gsub("_", "")
                    local x
                    if forceNumber then
                        x = tonumber(numberWithStrippedUnderscores) or
                            parseError('could not read number "' .. rawstr .. '"')
                    else
                        x = tonumber(numberWithStrippedUnderscores)
                        if not x then
                            if(rawstr:match("%.[0-9]")) then
                                byteindex = (byteindex - #rawstr +
                                                 rawstr:find("%.[0-9]") + 1)
                                parseError("can't start multisym segment " ..
                                               "with a digit: ".. rawstr)
                            elseif(rawstr:match("[%.:][%.:]") and
                                   rawstr ~= "..") then
                                byteindex = (byteindex - #rawstr +
                                                 rawstr:find("[%.:][%.:]") + 1)
                                parseError("malformed multisym: " .. rawstr)
                            elseif(rawstr:match(":.+[%.:]")) then
                                byteindex = (byteindex - #rawstr +
                                                 rawstr:find(":.+[%.:]"))
                                parseError("method must be last component " ..
                                               "of multisym: " .. rawstr)
                            else
                                x = sym(rawstr, nil, { line = line,
                                                       filename = filename,
                                                       bytestart = bytestart,
                                                       byteend = byteindex, })
                            end
                        end
                    end
                    dispatch(x)
                end
            else
                parseError("illegal character: " .. string.char(b))
            end
        until done
        return true, retval
    end, function ()
        stack = {}
    end
end

--
-- Compilation
--

-- Top level compilation bindings.
local rootChunk, rootScope, rootOptions

local function setResetRoot(oldChunk, oldScope, oldOptions)
    local oldResetRoot = resetRoot -- this needs to nest!
    resetRoot = function()
        rootChunk, rootScope, rootOptions = oldChunk, oldScope, oldOptions
        resetRoot = oldResetRoot
    end
end

local GLOBAL_SCOPE

-- Create a new Scope, optionally under a parent scope. Scopes are compile time
-- constructs that are responsible for keeping track of local variables, name
-- mangling, and macros.  They are accessible to user code via the
-- 'eval-compiler' special form (may change). They use metatables to implement
-- nesting.
local function makeScope(parent)
    if not parent then parent = GLOBAL_SCOPE end
    return {
        unmanglings = setmetatable({}, {
            __index = parent and parent.unmanglings
        }),
        manglings = setmetatable({}, {
            __index = parent and parent.manglings
        }),
        specials = setmetatable({}, {
            __index = parent and parent.specials
        }),
        macros = setmetatable({}, {
            __index = parent and parent.macros
        }),
        symmeta = setmetatable({}, {
            __index = parent and parent.symmeta
        }),
        includes = setmetatable({}, {
            __index = parent and parent.includes
        }),
        refedglobals = setmetatable({}, {
            __index = parent and parent.refedglobals
        }),
        autogensyms = {},
        parent = parent,
        vararg = parent and parent.vararg,
        depth = parent and ((parent.depth or 0) + 1) or 0,
        hashfn = parent and parent.hashfn
    }
end

-- Assert a condition and raise a compile error with line numbers. The ast arg
-- should be unmodified so that its first element is the form being called.
-- If you add new calls to this function, please update fenneldfriend.fnl
-- as well to add suggestions for how to fix the new error.
local function assertCompile(condition, msg, ast)
    local override = rootOptions and rootOptions["assert-compile"]
    if override then
        -- don't make custom handlers deal with resetting root; it's error-prone
        if not condition and resetRoot then resetRoot() end
        override(condition, msg, ast)
        -- should we fall thru to the default check, or should we allow the
        -- override to swallow the error?
    end
    if not condition then
        if resetRoot then resetRoot() end
        local m = getmetatable(ast)
        local filename = m and m.filename or ast.filename or "unknown"
        local line = m and m.line or ast.line or "?"
        -- if we use regular `assert' we can't provide the `level' argument of 0
        error(string.format("Compile error in '%s' %s:%s: %s",
                            tostring(isSym(ast[1]) and ast[1][1] or ast[1] or '()'),
                            filename, line, msg), 0)
    end
    return condition
end

GLOBAL_SCOPE = makeScope()
GLOBAL_SCOPE.vararg = true
local SPECIALS = GLOBAL_SCOPE.specials
local COMPILER_SCOPE = makeScope(GLOBAL_SCOPE)

local luaKeywords = {
    'and', 'break', 'do', 'else', 'elseif', 'end', 'false', 'for', 'function',
    'if', 'in', 'local', 'nil', 'not', 'or', 'repeat', 'return', 'then', 'true',
    'until', 'while'
}

for i, v in ipairs(luaKeywords) do luaKeywords[v] = i end

local function isValidLuaIdentifier(str)
    return (str:match('^[%a_][%w_]*$') and not luaKeywords[str])
end

-- Allow printing a string to Lua, also keep as 1 line.
local serializeSubst = {
    ['\a'] = '\\a',
    ['\b'] = '\\b',
    ['\f'] = '\\f',
    ['\n'] = 'n',
    ['\t'] = '\\t',
    ['\v'] = '\\v'
}
local function serializeString(str)
    local s = ("%q"):format(str)
    s = s:gsub('.', serializeSubst):gsub("[\128-\255]", function(c)
        return "\\" .. c:byte()
    end)
    return s
end

-- A multi symbol is a symbol that is actually composed of
-- two or more symbols using the dot syntax. The main differences
-- from normal symbols is that they cannot be declared local, and
-- they may have side effects on invocation (metatables)
local function isMultiSym(str)
    if isSym(str) then
        return isMultiSym(tostring(str))
    end
    if type(str) ~= 'string' then return end
    local parts = {}
    for part in str:gmatch('[^%.%:]+[%.%:]?') do
        local lastChar = part:sub(-1)
        if lastChar == ":" then
            parts.multiSymMethodCall = true
        end
        if lastChar == ":" or lastChar == "." then
            parts[#parts + 1] = part:sub(1, -2)
        else
            parts[#parts + 1] = part
        end
    end
    return #parts > 0 and
        (str:match('%.') or str:match(':')) and
        (not str:match('%.%.')) and
        str:byte() ~= string.byte '.' and
        str:byte(-1) ~= string.byte '.' and
        parts
end

local function isQuoted(symbol) return symbol.quoted end

-- Mangler for global symbols. Does not protect against collisions,
-- but makes them unlikely. This is the mangling that is exposed to
-- to the world.
local function globalMangling(str)
    if isValidLuaIdentifier(str) then
        return str
    end
    -- Use underscore as escape character
    return '__fnl_global__' .. str:gsub('[^%w]', function (c)
        return ('_%02x'):format(c:byte())
    end)
end

-- Reverse a global mangling. Takes a Lua identifier and
-- returns the fennel symbol string that created it.
local function globalUnmangling(identifier)
    local rest = identifier:match('^__fnl_global__(.*)$')
    if rest then
        local r = rest:gsub('_[%da-f][%da-f]', function (code)
            return string.char(tonumber(code:sub(2), 16))
        end)
        return r -- don't return multiple values
    else
        return identifier
    end
end

-- If there's a provided list of allowed globals, don't let references thru that
-- aren't on the list. This list is set at the compiler entry points of compile
-- and compileStream.
local allowedGlobals

local function globalAllowed(name)
    if not allowedGlobals then return true end
    for _, g in ipairs(allowedGlobals) do
        if g == name then return true end
    end
end

-- Creates a symbol from a string by mangling it.
-- ensures that the generated symbol is unique
-- if the input string is unique in the scope.
local function localMangling(str, scope, ast, tempManglings)
    local append = 0
    local mangling = str
    assertCompile(not isMultiSym(str), 'unexpected multi symbol ' .. str, ast)

    -- Mapping mangling to a valid Lua identifier
    if luaKeywords[mangling] or mangling:match('^%d') then
        mangling = '_' .. mangling
    end
    mangling = mangling:gsub('-', '_')
    mangling = mangling:gsub('[^%w_]', function (c)
        return ('_%02x'):format(c:byte())
    end)

    -- Prevent name collisions with existing symbols
    local raw = mangling
    while scope.unmanglings[mangling] do
        mangling = raw .. append
        append = append + 1
    end

    scope.unmanglings[mangling] = str
    local manglings = tempManglings or scope.manglings
    manglings[str] = mangling
    return mangling
end

-- Calling this function will mean that further
-- compilation in scope will use these new manglings
-- instead of the current manglings.
local function applyManglings(scope, newManglings, ast)
    for raw, mangled in pairs(newManglings) do
        assertCompile(not scope.refedglobals[mangled],
        "use of global " .. raw .. " is aliased by a local", ast)
        scope.manglings[raw] = mangled
    end
end

-- Combine parts of a symbol
local function combineParts(parts, scope)
    local ret = scope.manglings[parts[1]] or globalMangling(parts[1])
    for i = 2, #parts do
        if isValidLuaIdentifier(parts[i]) then
            if parts.multiSymMethodCall and i == #parts then
                ret = ret .. ':' .. parts[i]
            else
                ret = ret .. '.' .. parts[i]
            end
        else
            ret = ret .. '[' .. serializeString(parts[i]) .. ']'
        end
    end
    return ret
end

-- Generates a unique symbol in the scope.
local function gensym(scope, base)
    local mangling
    local append = 0
    repeat
        mangling = (base or '') .. '_' .. append .. '_'
        append = append + 1
    until not scope.unmanglings[mangling]
    scope.unmanglings[mangling] = true
    return mangling
end

-- Generates a unique symbol in the scope based on the base name. Calling
-- repeatedly with the same base and same scope will return existing symbol
-- rather than generating new one.
local function autogensym(base, scope)
    if scope.autogensyms[base] then return scope.autogensyms[base] end
    local mangling = gensym(scope, base)
    scope.autogensyms[base] = mangling
    return mangling
end

-- Check if a binding is valid
local function checkBindingValid(symbol, scope, ast)
    -- Check if symbol will be over shadowed by special
    local name = symbol[1]
    assertCompile(not scope.specials[name] and not scope.macros[name],
                  ("local %s was overshadowed by a special form or macro")
                      :format(name), ast)
    assertCompile(not isQuoted(symbol),
                  ("macro tried to bind %s without gensym"):format(name), symbol)

end

-- Declare a local symbol
local function declareLocal(symbol, meta, scope, ast, tempManglings)
    checkBindingValid(symbol, scope, ast)
    local name = symbol[1]
    assertCompile(not isMultiSym(name),
                  "unexpected multi symbol " .. name, ast)
    local mangling = localMangling(name, scope, ast, tempManglings)
    scope.symmeta[name] = meta
    return mangling
end

-- Convert symbol to Lua code. Will only work for local symbols
-- if they have already been declared via declareLocal
local function symbolToExpression(symbol, scope, isReference)
    local name = symbol[1]
    local multiSymParts = isMultiSym(name)
    if scope.hashfn then
       if name == '$' then name = '$1' end
       if multiSymParts then
          if multiSymParts[1] == "$" then
             multiSymParts[1] = "$1"
             name = table.concat(multiSymParts, ".")
          end
       end
    end
    local parts = multiSymParts or {name}
    local etype = (#parts > 1) and "expression" or "sym"
    local isLocal = scope.manglings[parts[1]]
    if isLocal and scope.symmeta[parts[1]] then scope.symmeta[parts[1]].used = true end
    -- if it's a reference and not a symbol which introduces a new binding
    -- then we need to check for allowed globals
    assertCompile(not isReference or isLocal or globalAllowed(parts[1]),
                  'unknown global in strict mode: ' .. parts[1], symbol)
    if not isLocal then
        rootScope.refedglobals[parts[1]] = true
    end
    return expr(combineParts(parts, scope), etype)
end


-- Emit Lua code
local function emit(chunk, out, ast)
    if type(out) == 'table' then
        table.insert(chunk, out)
    else
        table.insert(chunk, {leaf = out, ast = ast})
    end
end

-- Do some peephole optimization.
local function peephole(chunk)
    if chunk.leaf then return chunk end
    -- Optimize do ... end in some cases.
    if #chunk >= 3 and
        chunk[#chunk - 2].leaf == 'do' and
        not chunk[#chunk - 1].leaf and
        chunk[#chunk].leaf == 'end' then
        local kid = peephole(chunk[#chunk - 1])
        local newChunk = {ast = chunk.ast}
        for i = 1, #chunk - 3 do table.insert(newChunk, peephole(chunk[i])) end
        for i = 1, #kid do table.insert(newChunk, kid[i]) end
        return newChunk
    end
    -- Recurse
    return map(chunk, peephole)
end

-- correlate line numbers in input with line numbers in output
local function flattenChunkCorrelated(mainChunk)
    local function flatten(chunk, out, lastLine, file)
        if chunk.leaf then
            out[lastLine] = (out[lastLine] or "") .. " " .. chunk.leaf
        else
            for _, subchunk in ipairs(chunk) do
                -- Ignore empty chunks
                if subchunk.leaf or #subchunk > 0 then
                    -- don't increase line unless it's from the same file
                    if subchunk.ast and file == subchunk.ast.file then
                        lastLine = math.max(lastLine, subchunk.ast.line or 0)
                    end
                    lastLine = flatten(subchunk, out, lastLine, file)
                end
            end
        end
        return lastLine
    end
    local out = {}
    local last = flatten(mainChunk, out, 1, mainChunk.file)
    for i = 1, last do
        if out[i] == nil then out[i] = "" end
    end
    return table.concat(out, "\n")
end

-- Flatten a tree of indented Lua source code lines.
-- Tab is what is used to indent a block.
local function flattenChunk(sm, chunk, tab, depth)
    if type(tab) == 'boolean' then tab = tab and '  ' or '' end
    if chunk.leaf then
        local code = chunk.leaf
        local info = chunk.ast
        -- Just do line info for now to save memory
        if sm then sm[#sm + 1] = info and info.line or -1 end
        return code
    else
        local parts = map(chunk, function(c)
            if c.leaf or #c > 0 then -- Ignore empty chunks
                local sub = flattenChunk(sm, c, tab, depth + 1)
                if depth > 0 then sub = tab .. sub:gsub('\n', '\n' .. tab) end
                return sub
            end
        end)
        return table.concat(parts, '\n')
    end
end

-- Some global state for all fennel sourcemaps. For the time being,
-- this seems the easiest way to store the source maps.
-- Sourcemaps are stored with source being mapped as the key, prepended
-- with '@' if it is a filename (like debug.getinfo returns for source).
-- The value is an array of mappings for each line.
local fennelSourcemap = {}
-- TODO: loading, unloading, and saving sourcemaps?

local function makeShortSrc(source)
    source = source:gsub('\n', ' ')
    if #source <= 49 then
        return '[fennel "' .. source .. '"]'
    else
        return '[fennel "' .. source:sub(1, 46) .. '..."]'
    end
end

-- Return Lua source and source map table
local function flatten(chunk, options)
    chunk = peephole(chunk)
    if(options.correlate) then
        return flattenChunkCorrelated(chunk), {}
    else
        local sm = {}
        local ret = flattenChunk(sm, chunk, options.indent, 0)
        if sm then
            local key, short_src
            if options.filename then
                short_src = options.filename
                key = '@' .. short_src
            else
                key = ret
                short_src = makeShortSrc(options.source or ret)
            end
            sm.short_src = short_src
            sm.key = key
            fennelSourcemap[key] = sm
        end
        return ret, sm
    end
end

-- module-wide state for metadata
-- create metadata table with weakly-referenced keys
local function makeMetadata()
    return setmetatable({}, {
        __mode = 'k',
        __index = {
            get = function(self, tgt, key)
                if self[tgt] then return self[tgt][key] end
            end,
            set = function(self, tgt, key, value)
                self[tgt] = self[tgt] or {}
                self[tgt][key] = value
                return tgt
            end,
            setall = function(self, tgt, ...)
                local kvLen, kvs = select('#', ...), {...}
                if kvLen % 2 ~= 0 then
                    error('metadata:setall() expected even number of k/v pairs')
                end
                self[tgt] = self[tgt] or {}
                for i = 1, kvLen, 2 do self[tgt][kvs[i]] = kvs[i + 1] end
                return tgt
            end,
        }})
end

local metadata = makeMetadata()
local doc = function(tgt, name)
    if(not tgt) then return name .. " not found" end
    local docstring = (metadata:get(tgt, 'fnl/docstring') or
                           '#<undocumented>'):gsub('\n$', ''):gsub('\n', '\n  ')
    if type(tgt) == "function" then
        local arglist = table.concat(metadata:get(tgt, 'fnl/arglist') or
                                         {'#<unknown-arguments>'}, ' ')
        return string.format("(%s%s%s)\n  %s", name, #arglist > 0 and ' ' or '',
                             arglist, docstring)
    else
        return string.format("%s\n  %s", name, docstring)
    end
end

local function docSpecial(name, arglist, docstring)
    metadata[SPECIALS[name]] =
        { ["fnl/docstring"] = docstring, ["fnl/arglist"] = arglist }
end

-- Convert expressions to Lua string
local function exprs1(exprs)
    return table.concat(map(exprs, 1), ', ')
end

-- Compile side effects for a chunk
local function keepSideEffects(exprs, chunk, start, ast)
    start = start or 1
    for j = start, #exprs do
        local se = exprs[j]
        -- Avoid the rogue 'nil' expression (nil is usually a literal,
        -- but becomes an expression if a special form
        -- returns 'nil'.)
        if se.type == 'expression' and se[1] ~= 'nil' then
            emit(chunk, ('do local _ = %s end'):format(tostring(se)), ast)
        elseif se.type == 'statement' then
            local code = tostring(se)
            emit(chunk, code:byte() == 40 and ("do end " .. code) or code , ast)
        end
    end
end

-- Does some common handling of returns and register
-- targets for special forms. Also ensures a list expression
-- has an acceptable number of expressions if opts contains the
-- "nval" option.
local function handleCompileOpts(exprs, parent, opts, ast)
    if opts.nval then
        local n = opts.nval
        if n ~= #exprs then
            local len = #exprs
            if len > n then
                -- Drop extra
                keepSideEffects(exprs, parent, n + 1, ast)
                for i = n + 1, len do
                    exprs[i] = nil
                end
            else
                -- Pad with nils
                for i = #exprs + 1, n do
                    exprs[i] = expr('nil', 'literal')
                end
            end
        end
    end
    if opts.tail then
        emit(parent, ('return %s'):format(exprs1(exprs)), ast)
    end
    if opts.target then
        local result = exprs1(exprs)
        if result == '' then result = 'nil' end
        emit(parent, ('%s = %s'):format(opts.target, result), ast)
    end
    if opts.tail or opts.target then
        -- Prevent statements and expression from being used twice if they
        -- have side-effects. Since if the target or tail options are set,
        -- the expressions are already emitted, we should not return them. This
        -- is fine, as when these options are set, the caller doesn't need the result
        -- anyways.
        exprs = {}
    end
    return exprs
end

-- Save current macro scope; used by gensym, in-scope?, etc
local macroCurrentScope = GLOBAL_SCOPE

local function macroexpand(ast, scope, once)
    if not isList(ast) then return ast end -- bail early if not a list form
    local multiSymParts = isMultiSym(ast[1])
    local macro = isSym(ast[1]) and scope.macros[deref(ast[1])]
    if not macro and multiSymParts then
        local inMacroModule
        macro = scope.macros
        for i = 1, #multiSymParts do
            macro = isTable(macro) and macro[multiSymParts[i]]
            if macro then inMacroModule = true end
        end
        assertCompile(not inMacroModule or type(macro) == 'function',
            'macro not found in imported macro module', ast)
    end
    if not macro then return ast end
    local oldScope = macroCurrentScope
    macroCurrentScope = scope
    local ok, transformed = pcall(macro, unpack(ast, 2))
    macroCurrentScope = oldScope
    assertCompile(ok, transformed, ast)
    if once or not transformed then return transformed end -- macroexpand-1
    return macroexpand(transformed, scope)
end

-- Compile an AST expression in the scope into parent, a tree
-- of lines that is eventually compiled into Lua code. Also
-- returns some information about the evaluation of the compiled expression,
-- which can be used by the calling function. Macros
-- are resolved here, as well as special forms in that order.
-- the 'ast' param is the root AST to compile
-- the 'scope' param is the scope in which we are compiling
-- the 'parent' param is the table of lines that we are compiling into.
-- add lines to parent by appending strings. Add indented blocks by appending
-- tables of more lines.
-- the 'opts' param contains info about where the form is being compiled.
-- Options include:
--   'target' - mangled name of symbol(s) being compiled to.
--      Could be one variable, 'a', or a list, like 'a, b, _0_'.
--   'tail' - boolean indicating tail position if set. If set, form will generate a return
--   instruction.
--   'nval' - The number of values to compile to if it is known to be a fixed value.

-- In Lua, an expression can evaluate to 0 or more values via multiple
-- returns. In many cases, Lua will drop extra values and convert a 0 value
-- expression to nil. In other cases, Lua will use all of the values in an
-- expression, such as in the last argument of a function call. Nval is an
-- option passed to compile1 to say that the resulting expression should have
-- at least n values. It lets us generate better code, because if we know we
-- are only going to use 1 or 2 values from an expression, we can create 1 or 2
-- locals to store intermediate results rather than turn the expression into a
-- closure that is called immediately, which we have to do if we don't know.

local function compile1(ast, scope, parent, opts)
    opts = opts or {}
    local exprs = {}
    -- expand any top-level macros before parsing and emitting Lua
    ast = macroexpand(ast, scope)
    -- Compile the form
    if isList(ast) then -- Function call or special form
        assertCompile(#ast > 0, "expected a function, macro, or special to call", ast)
        -- Test for special form
        local len, first = #ast, ast[1]
        local multiSymParts = isMultiSym(first)
        local special = isSym(first) and scope.specials[deref(first)]
        if special then -- Special form
            exprs = special(ast, scope, parent, opts) or expr('nil', 'literal')
            -- Be very accepting of strings or expression
            -- as well as lists or expressions
            if type(exprs) == 'string' then exprs = expr(exprs, 'expression') end
            if getmetatable(exprs) == EXPR_MT then exprs = {exprs} end
            -- Unless the special form explicitly handles the target, tail, and
            -- nval properties, (indicated via the 'returned' flag), handle
            -- these options.
            if not exprs.returned then
                exprs = handleCompileOpts(exprs, parent, opts, ast)
            elseif opts.tail or opts.target then
                exprs = {}
            end
            exprs.returned = true
            return exprs
        elseif multiSymParts and multiSymParts.multiSymMethodCall then
            local tableWithMethod = table.concat({
                    unpack(multiSymParts, 1, #multiSymParts - 1)
                                                 }, '.')
            local methodToCall = multiSymParts[#multiSymParts]
            local newAST = list(sym(':', scope), sym(tableWithMethod, scope), methodToCall)
            for i = 2, len do
                newAST[#newAST + 1] = ast[i]
            end
            local compiled = compile1(newAST, scope, parent, opts)
            exprs = compiled
        else -- Function call
            local fargs = {}
            local fcallee = compile1(ast[1], scope, parent, {
                nval = 1
            })[1]
            assertCompile(fcallee.type ~= 'literal',
                          'cannot call literal value', ast)
            fcallee = tostring(fcallee)
            for i = 2, len do
                local subexprs = compile1(ast[i], scope, parent, {
                    nval = i ~= len and 1 or nil
                })
                fargs[#fargs + 1] = subexprs[1] or expr('nil', 'literal')
                if i == len then
                    -- Add sub expressions to function args
                    for j = 2, #subexprs do
                        fargs[#fargs + 1] = subexprs[j]
                    end
                else
                    -- Emit sub expression only for side effects
                    keepSideEffects(subexprs, parent, 2, ast[i])
                end
            end
            local call = ('%s(%s)'):format(tostring(fcallee), exprs1(fargs))
            exprs = handleCompileOpts({expr(call, 'statement')}, parent, opts, ast)
        end
    elseif isVarg(ast) then
        assertCompile(scope.vararg, "unexpected vararg", ast)
        exprs = handleCompileOpts({expr('...', 'varg')}, parent, opts, ast)
    elseif isSym(ast) then
        local e
        local multiSymParts = isMultiSym(ast)
        assertCompile(not (multiSymParts and multiSymParts.multiSymMethodCall),
                      "multisym method calls may only be in call position", ast)
        -- Handle nil as special symbol - it resolves to the nil literal rather than
        -- being unmangled. Alternatively, we could remove it from the lua keywords table.
        if ast[1] == 'nil' then
            e = expr('nil', 'literal')
        else
            e = symbolToExpression(ast, scope, true)
        end
        exprs = handleCompileOpts({e}, parent, opts, ast)
    elseif type(ast) == 'nil' or type(ast) == 'boolean' then
        exprs = handleCompileOpts({expr(tostring(ast), 'literal')}, parent, opts)
    elseif type(ast) == 'number' then
        local n = ('%.17g'):format(ast)
        exprs = handleCompileOpts({expr(n, 'literal')}, parent, opts)
    elseif type(ast) == 'string' then
        local s = serializeString(ast)
        exprs = handleCompileOpts({expr(s, 'literal')}, parent, opts)
    elseif type(ast) == 'table' then
        local buffer = {}
        for i = 1, #ast do -- Write numeric keyed values.
            local nval = i ~= #ast and 1
            buffer[#buffer + 1] = exprs1(compile1(ast[i], scope, parent, {nval = nval}))
        end
        local function writeOtherValues(k)
            if type(k) ~= 'number' or math.floor(k) ~= k or k < 1 or k > #ast then
                if type(k) == 'string' and isValidLuaIdentifier(k) then
                    return {k, k}
                else
                    local kstr = '[' .. tostring(compile1(k, scope, parent,
                                                          {nval = 1})[1]) .. ']'
                    return { kstr, k }
                end
            end
        end
        local keys = kvmap(ast, writeOtherValues)
        table.sort(keys, function (a, b) return a[1] < b[1] end)
        map(keys, function(k)
                local v = tostring(compile1(ast[k[2]], scope, parent, {nval = 1})[1])
                return ('%s = %s'):format(k[1], v) end,
            buffer)
        local tbl = '{' .. table.concat(buffer, ', ') ..'}'
        exprs = handleCompileOpts({expr(tbl, 'expression')}, parent, opts, ast)
    else
        assertCompile(false, 'could not compile value of type ' .. type(ast), ast)
    end
    exprs.returned = true
    return exprs
end

-- SPECIALS --

-- For statements and expressions, put the value in a local to avoid
-- double-evaluating it.
local function once(val, ast, scope, parent)
    if val.type == 'statement' or val.type == 'expression' then
        local s = gensym(scope)
        emit(parent, ('local %s = %s'):format(s, tostring(val)), ast)
        return expr(s, 'sym')
    else
        return val
    end
end

-- Implements destructuring for forms like let, bindings, etc.
-- Takes a number of options to control behavior.
-- var: Whether or not to mark symbols as mutable
-- declaration: begin each assignment with 'local' in output
-- nomulti: disallow multisyms in the destructuring. Used for (local) and (global).
-- noundef: Don't set undefined bindings. (set)
-- forceglobal: Don't allow local bindings
local function destructure(to, from, ast, scope, parent, opts)
    opts = opts or {}
    local isvar = opts.isvar
    local declaration = opts.declaration
    local nomulti = opts.nomulti
    local noundef = opts.noundef
    local forceglobal = opts.forceglobal
    local forceset = opts.forceset
    local setter = declaration and "local %s = %s" or "%s = %s"

    local newManglings = {}

    -- Get Lua source for symbol, and check for errors
    local function getname(symbol, up1)
        local raw = symbol[1]
        assertCompile(not (nomulti and isMultiSym(raw)),
            'unexpected multi symbol ' .. raw, up1)
        if declaration then
            return declareLocal(symbol, {var = isvar}, scope, symbol, newManglings)
        else
            local parts = isMultiSym(raw) or {raw}
            local meta = scope.symmeta[parts[1]]
            if #parts == 1 and not forceset then
                assertCompile(not(forceglobal and meta),
                    ("global %s conflicts with local"):format(tostring(symbol)), symbol)
                assertCompile(not (meta and not meta.var),
                    'expected var ' .. raw, symbol)
                assertCompile(meta or not noundef,
                    'expected local ' .. parts[1], symbol)
            end
            if forceglobal then
                assertCompile(not scope.symmeta[scope.unmanglings[raw]],
                              "global " .. raw .. " conflicts with local", symbol)
                scope.manglings[raw] = globalMangling(raw)
                scope.unmanglings[globalMangling(raw)] = raw
                if allowedGlobals then
                    table.insert(allowedGlobals, raw)
                end
            end

            return symbolToExpression(symbol, scope)[1]
        end
    end

    -- Compile the outer most form. We can generate better Lua in this case.
    local function compileTopTarget(lvalues)
        -- Calculate initial rvalue
        local inits = map(lvalues, function(x)
                              return scope.manglings[x] and x or 'nil' end)
        local init = table.concat(inits, ', ')
        local lvalue = table.concat(lvalues, ', ')

        local plen = #parent
        local ret = compile1(from, scope, parent, {target = lvalue})
        if declaration then
            if #parent == plen + 1 and parent[#parent].leaf then
                -- A single leaf emitted means an simple assignment a = x was emitted
                parent[#parent].leaf = 'local ' .. parent[#parent].leaf
            else
                table.insert(parent, plen + 1, { leaf = 'local ' .. lvalue ..
                                                     ' = ' .. init, ast = ast})
            end
        end
        return ret
    end

    -- Recursive auxiliary function
    local function destructure1(left, rightexprs, up1, top)
        if isSym(left) and left[1] ~= "nil" then
            checkBindingValid(left, scope, left)
            local lname = getname(left, up1)
            if top then
                compileTopTarget({lname})
            else
                emit(parent, setter:format(lname, exprs1(rightexprs)), left)
            end
        elseif isTable(left) then -- table destructuring
            if top then rightexprs = compile1(from, scope, parent) end
            local s = gensym(scope)
            local right = exprs1(rightexprs)
            if right == '' then right = 'nil' end
            emit(parent, ("local %s = %s"):format(s, right), left)
            for k, v in stablepairs(left) do
                if isSym(left[k]) and left[k][1] == "&" then
                    assertCompile(type(k) == "number" and not left[k+2],
                        "expected rest argument before last parameter", left)
                    local subexpr = expr(('{(table.unpack or unpack)(%s, %s)}'):format(s, k),
                        'expression')
                    destructure1(left[k+1], {subexpr}, left)
                    return
                else
                    if isSym(k) and tostring(k) == ":" and isSym(v) then k = tostring(v) end
                    if type(k) ~= "number" then k = serializeString(k) end
                    local subexpr = expr(('%s[%s]'):format(s, k), 'expression')
                    destructure1(v, {subexpr}, left)
                end
            end
        elseif isList(left) then -- values destructuring
            local leftNames, tables = {}, {}
            for i, name in ipairs(left) do
                local symname
                if isSym(name) then -- binding directly to a name
                    symname = getname(name, up1)
                else -- further destructuring of tables inside values
                    symname = gensym(scope)
                    tables[i] = {name, expr(symname, 'sym')}
                end
                table.insert(leftNames, symname)
            end
            if top then
                compileTopTarget(leftNames)
            else
                local lvalue = table.concat(leftNames, ', ')
                emit(parent, setter:format(lvalue, exprs1(rightexprs)), left)
            end
            for _, pair in stablepairs(tables) do -- recurse if left-side tables found
                destructure1(pair[1], {pair[2]}, left)
            end
        else
            assertCompile(false, ("unable to bind %s %s"):
                              format(type(left), tostring(left)),
                          type(up1[2]) == "table" and up1[2] or up1)
        end
        if top then return {returned = true} end
    end

    local ret = destructure1(to, nil, ast, true)
    applyManglings(scope, newManglings, ast)
    return ret
end

-- Unlike most expressions and specials, 'values' resolves with multiple
-- values, one for each argument, allowing multiple return values. The last
-- expression can return multiple arguments as well, allowing for more than the number
-- of expected arguments.
local function values(ast, scope, parent)
    local len = #ast
    local exprs = {}
    for i = 2, len do
        local subexprs = compile1(ast[i], scope, parent, {
            nval = (i ~= len) and 1
        })
        exprs[#exprs + 1] = subexprs[1]
        if i == len then
            for j = 2, #subexprs do
                exprs[#exprs + 1] = subexprs[j]
            end
        end
    end
    return exprs
end

-- Compile a list of forms for side effects
local function compileDo(ast, scope, parent, start)
    start = start or 2
    local len = #ast
    local subScope = makeScope(scope)
    for i = start, len do
        compile1(ast[i], subScope, parent, {
            nval = 0
        })
    end
end

-- Raises compile error if unused locals are found and we're checking for them.
local function checkUnused(scope, ast)
    if not rootOptions.checkUnusedLocals then return end
    for symName in pairs(scope.symmeta) do
        assertCompile(scope.symmeta[symName].used or symName:find("^_"),
                      -- ast here is the whole form, not the local itself
                      ("unused local %s"):format(symName), ast)
    end
end

-- Implements a do statement, starting at the 'start' element. By default, start is 2.
local function doImpl(ast, scope, parent, opts, start, chunk, subScope, preSyms)
    start = start or 2
    subScope = subScope or makeScope(scope)
    chunk = chunk or {}
    local len = #ast
    local outerTarget = opts.target
    local outerTail = opts.tail
    local retexprs = {returned = true}

    -- See if we need special handling to get the return values
    -- of the do block
    if not outerTarget and opts.nval ~= 0 and not outerTail then
        if opts.nval then
            -- Generate a local target
            local syms = {}
            for i = 1, opts.nval do
                local s = preSyms and preSyms[i] or gensym(scope)
                syms[i] = s
                retexprs[i] = expr(s, 'sym')
            end
            outerTarget = table.concat(syms, ', ')
            emit(parent, ('local %s'):format(outerTarget), ast)
            emit(parent, 'do', ast)
        else
            -- We will use an IIFE for the do
            local fname = gensym(scope)
            local fargs = scope.vararg and '...' or ''
            emit(parent, ('local function %s(%s)'):format(fname, fargs), ast)
            retexprs = expr(fname .. '(' .. fargs .. ')', 'statement')
            outerTail = true
            outerTarget = nil
        end
    else
        emit(parent, 'do', ast)
    end
    -- Compile the body
    if start > len then
        -- In the unlikely case we do a do with no arguments.
        compile1(nil, subScope, chunk, {
            tail = outerTail,
            target = outerTarget
        })
        -- There will be no side effects
    else
        for i = start, len do
            local subopts = {
                nval = i ~= len and 0 or opts.nval,
                tail = i == len and outerTail or nil,
                target = i == len and outerTarget or nil
            }
            local subexprs = compile1(ast[i], subScope, chunk, subopts)
            if i ~= len then
                keepSideEffects(subexprs, parent, nil, ast[i])
            end
        end
    end
    emit(parent, chunk, ast)
    emit(parent, 'end', ast)
    checkUnused(subScope, ast)
    return retexprs
end

SPECIALS["do"] = doImpl
docSpecial("do", {"..."}, "Evaluate multiple forms; return last value.")

SPECIALS["values"] = values
docSpecial("values", {"..."},
           "Return multiple values from a function.  Must be in tail position.")

-- The fn special declares a function. Syntax is similar to other lisps;
-- (fn optional-name [arg ...] (body))
-- Further decoration such as docstrings, meta info, and multibody functions a possibility.
SPECIALS["fn"] = function(ast, scope, parent)
    local fScope = makeScope(scope)
    local fChunk = {}
    local index = 2
    local fnName = isSym(ast[index])
    local isLocalFn
    local docstring
    fScope.vararg = false
    local multi = fnName and isMultiSym(fnName[1])
    assertCompile(not multi or not multi.multiSymMethodCall,
                  "unexpected multi symbol " .. tostring(fnName), ast[index])
    if fnName and fnName[1] ~= 'nil' then
        isLocalFn = not multi
        if isLocalFn then
            fnName = declareLocal(fnName, {}, scope, ast)
        else
            fnName = symbolToExpression(fnName, scope)[1]
        end
        index = index + 1
    else
        isLocalFn = true
        fnName = gensym(scope)
    end
    local argList = assertCompile(isTable(ast[index]),
                                  "expected parameters",
                                  type(ast[index]) == "table" and ast[index] or ast)
    local function getArgName(i, name)
        if isVarg(name) then
            assertCompile(i == #argList, "expected vararg as last parameter", ast[2])
            fScope.vararg = true
            return "..."
        elseif(isSym(name) and deref(name) ~= "nil"
               and not isMultiSym(deref(name))) then
            return declareLocal(name, {}, fScope, ast)
        elseif isTable(name) then
            local raw = sym(gensym(scope))
            local declared = declareLocal(raw, {}, fScope, ast)
            destructure(name, raw, ast, fScope, fChunk,
                        { declaration = true, nomulti = true })
            return declared
        else
            assertCompile(false, ("expected symbol for function parameter: %s"):
                              format(tostring(name)), ast[2])
        end
    end
    local argNameList = kvmap(argList, getArgName)
    if type(ast[index + 1]) == 'string' and index + 1 < #ast then
        index = index + 1
        docstring = ast[index]
    end
    for i = index + 1, #ast do
        compile1(ast[i], fScope, fChunk, {
            tail = i == #ast,
            nval = i ~= #ast and 0 or nil,
        })
    end
    if isLocalFn then
        emit(parent, ('local function %s(%s)')
                 :format(fnName, table.concat(argNameList, ', ')), ast)
    else
        emit(parent, ('%s = function(%s)')
                 :format(fnName, table.concat(argNameList, ', ')), ast)
    end

    emit(parent, fChunk, ast)
    emit(parent, 'end', ast)

    if rootOptions.useMetadata then
        local args = map(argList, function(v)
            -- TODO: show destructured args properly instead of replacing
            return isTable(v) and '"#<table>"' or string.format('"%s"', tostring(v))
        end)

        local metaFields = {
            '"fnl/arglist"', '{' .. table.concat(args, ', ') .. '}',
        }
        if docstring then
            table.insert(metaFields, '"fnl/docstring"')
            table.insert(metaFields, '"' .. docstring:gsub('%s+$', '')
                             :gsub('\\', '\\\\'):gsub('\n', '\\n')
                             :gsub('"', '\\"') .. '"')
        end
        local metaStr = ('require("%s").metadata'):format(rootOptions.moduleName or "fennel")
        emit(parent, string.format('pcall(function() %s:setall(%s, %s) end)',
                                   metaStr, fnName, table.concat(metaFields, ', ')))
    end

    checkUnused(fScope, ast)
    return expr(fnName, 'sym')
end
docSpecial("fn", {"name?", "args", "docstring?", "..."},
           "Function syntax. May optionally include a name and docstring."
               .."\nIf a name is provided, the function will be bound in the current scope."
               .."\nWhen called with the wrong number of args, excess args will be discarded"
               .."\nand lacking args will be nil, use lambda for arity-checked functions.")

-- (lua "print('hello!')") -> prints hello, evaluates to nil
-- (lua "print 'hello!'" "10") -> prints hello, evaluates to the number 10
-- (lua nil "{1,2,3}") -> Evaluates to a table literal
SPECIALS['lua'] = function(ast, _, parent)
    assertCompile(#ast == 2 or #ast == 3, "expected 1 or 2 arguments", ast)
    if ast[2] ~= nil then
        table.insert(parent, {leaf = tostring(ast[2]), ast = ast})
    end
    if #ast == 3 then
        return tostring(ast[3])
    end
end

SPECIALS['doc'] = function(ast, scope, parent)
    assert(rootOptions.useMetadata, "can't look up doc with metadata disabled.")
    assertCompile(#ast == 2, "expected one argument", ast)

    local target = deref(ast[2])
    local specialOrMacro = scope.specials[target] or scope.macros[target]
    if specialOrMacro then
        return ("print([[%s]])"):format(doc(specialOrMacro, target))
    else
        local value = tostring(compile1(ast[2], scope, parent, {nval = 1})[1])
        -- need to require here since the metadata is stored in the module
        -- and we need to make sure we look it up in the same module it was
        -- declared from.
        return ("print(require('%s').doc(%s, '%s'))")
            :format(rootOptions.moduleName or "fennel", value, tostring(ast[2]))
    end
end
docSpecial("doc", {"x"},
           "Print the docstring and arglist for a function, macro, or special form.")

-- Table lookup
SPECIALS["."] = function(ast, scope, parent)
    local len = #ast
    assertCompile(len > 1, "expected table argument", ast)
    local lhs = compile1(ast[2], scope, parent, {nval = 1})
    if len == 2 then
        return tostring(lhs[1])
    else
        local indices = {}
        for i = 3, len do
            local index = ast[i]
            if type(index) == 'string' and isValidLuaIdentifier(index) then
                table.insert(indices, '.' .. index)
            else
                index = compile1(index, scope, parent, {nval = 1})[1]
                table.insert(indices, '[' .. tostring(index) .. ']')
            end
        end
        -- extra parens are needed for table literals
        if isTable(ast[2]) then
            return '(' .. tostring(lhs[1]) .. ')' .. table.concat(indices)
        else
            return tostring(lhs[1]) .. table.concat(indices)
        end
    end
end
docSpecial(".", {"tbl", "key1", "..."},
           "Look up key1 in tbl table. If more args are provided, do a nested lookup.")

SPECIALS["global"] = function(ast, scope, parent)
    assertCompile(#ast == 3, "expected name and value", ast)
    destructure(ast[2], ast[3], ast, scope, parent, {
        nomulti = true,
        forceglobal = true
    })
end
docSpecial("global", {"name", "val"}, "Set name as a global with val.")

SPECIALS["set"] = function(ast, scope, parent)
    assertCompile(#ast == 3, "expected name and value", ast)
    destructure(ast[2], ast[3], ast, scope, parent, {
        noundef = true
    })
end
docSpecial("set", {"name", "val"},
           "Set a local variable to a new value. Only works on locals using var.")

SPECIALS["set-forcibly!"] = function(ast, scope, parent)
    assertCompile(#ast == 3, "expected name and value", ast)
    destructure(ast[2], ast[3], ast, scope, parent, {
        forceset = true
    })
end

SPECIALS["local"] = function(ast, scope, parent)
    assertCompile(#ast == 3, "expected name and value", ast)
    destructure(ast[2], ast[3], ast, scope, parent, {
        declaration = true,
        nomulti = true
    })
end
docSpecial("local", {"name", "val"},
           "Introduce new top-level immutable local.")

SPECIALS["var"] = function(ast, scope, parent)
    assertCompile(#ast == 3, "expected name and value", ast)
    destructure(ast[2], ast[3], ast, scope, parent, {
        declaration = true,
        nomulti = true,
        isvar = true
    })
end
docSpecial("var", {"name", "val"},
           "Introduce new mutable local.")

SPECIALS["let"] = function(ast, scope, parent, opts)
    local bindings = ast[2]
    assertCompile(isList(bindings) or isTable(bindings),
                  "expected binding table", ast)
    assertCompile(#bindings % 2 == 0,
                  "expected even number of name/value bindings", ast[2])
    assertCompile(#ast >= 3, "expected body expression", ast[1])
    -- we have to gensym the binding for the let body's return value before
    -- compiling the binding vector, otherwise there's a possibility to conflict
    local preSyms = {}
    for _ = 1, (opts.nval or 0) do table.insert(preSyms, gensym(scope)) end
    local subScope = makeScope(scope)
    local subChunk = {}
    for i = 1, #bindings, 2 do
        destructure(bindings[i], bindings[i + 1], ast, subScope, subChunk, {
            declaration = true,
            nomulti = true
        })
    end
    return doImpl(ast, scope, parent, opts, 3, subChunk, subScope, preSyms)
end
docSpecial("let", {"[name1 val1 ... nameN valN]", "..."},
           "Introduces a new scope in which a given set of local bindings are used.")

-- For setting items in a table
SPECIALS["tset"] = function(ast, scope, parent)
    assertCompile(#ast > 3, ("expected table, key, and value arguments"), ast)
    local root = compile1(ast[2], scope, parent, {nval = 1})[1]
    local keys = {}
    for i = 3, #ast - 1 do
        local key = compile1(ast[i], scope, parent, {nval = 1})[1]
        keys[#keys + 1] = tostring(key)
    end
    local value = compile1(ast[#ast], scope, parent, {nval = 1})[1]
    local rootstr = tostring(root)
    -- Prefix 'do end ' so parens are not ambiguous (grouping or function call?)
    local fmtstr = (rootstr:match("^{")) and "do end (%s)[%s] = %s" or "%s[%s] = %s"
    emit(parent, fmtstr:format(tostring(root),
                               table.concat(keys, ']['),
                               tostring(value)), ast)
end
docSpecial("tset", {"tbl", "key1", "...", "keyN", "val"},
           "Set the value of a table field. Can take additional keys to set"
        .. "nested values,\nbut all parents must contain an existing table.")

-- The if special form behaves like the cond form in
-- many languages
SPECIALS["if"] = function(ast, scope, parent, opts)
    local doScope = makeScope(scope)
    local branches = {}
    local elseBranch = nil

    -- Calculate some external stuff. Optimizes for tail calls and what not
    local wrapper, innerTail, innerTarget, targetExprs
    if opts.tail or opts.target or opts.nval then
        if opts.nval and opts.nval ~= 0 and not opts.target then
            -- We need to create a target
            targetExprs = {}
            local accum = {}
            for i = 1, opts.nval do
                local s = gensym(scope)
                accum[i] = s
                targetExprs[i] = expr(s, 'sym')
            end
            wrapper = 'target'
            innerTail = opts.tail
            innerTarget = table.concat(accum, ', ')
        else
            wrapper = 'none'
            innerTail = opts.tail
            innerTarget = opts.target
        end
    else
        wrapper = 'iife'
        innerTail = true
        innerTarget = nil
    end

    -- Compile bodies and conditions
    local bodyOpts = {
        tail = innerTail,
        target = innerTarget,
        nval = opts.nval
    }
    local function compileBody(i)
        local chunk = {}
        local cscope = makeScope(doScope)
        keepSideEffects(compile1(ast[i], cscope, chunk, bodyOpts),
        chunk, nil, ast[i])
        return {
            chunk = chunk,
            scope = cscope
        }
    end
    for i = 2, #ast - 1, 2 do
        local condchunk = {}
        local res = compile1(ast[i], doScope, condchunk, {nval = 1})
        local cond = res[1]
        local branch = compileBody(i + 1)
        branch.cond = cond
        branch.condchunk = condchunk
        branch.nested = i ~= 2 and next(condchunk, nil) == nil
        table.insert(branches, branch)
    end
    local hasElse = #ast > 3 and #ast % 2 == 0
    if hasElse then elseBranch = compileBody(#ast) end

    -- Emit code
    local s = gensym(scope)
    local buffer = {}
    local lastBuffer = buffer
    for i = 1, #branches do
        local branch = branches[i]
        local fstr = not branch.nested and 'if %s then' or 'elseif %s then'
        local cond = tostring(branch.cond)
        local condLine = (cond == "true" and branch.nested and i == #branches)
            and "else"
            or fstr:format(cond)
        if branch.nested then
            emit(lastBuffer, branch.condchunk, ast)
        else
            for _, v in ipairs(branch.condchunk) do emit(lastBuffer, v, ast) end
        end
        emit(lastBuffer, condLine, ast)
        emit(lastBuffer, branch.chunk, ast)
        if i == #branches then
            if hasElse then
                emit(lastBuffer, 'else', ast)
                emit(lastBuffer, elseBranch.chunk, ast)
            -- TODO: Consolidate use of condLine ~= "else" with hasElse
            elseif(innerTarget and condLine ~= 'else') then
                emit(lastBuffer, 'else', ast)
                emit(lastBuffer, ("%s = nil"):format(innerTarget), ast)
            end
            emit(lastBuffer, 'end', ast)
        elseif not branches[i + 1].nested then
            emit(lastBuffer, 'else', ast)
            local nextBuffer = {}
            emit(lastBuffer, nextBuffer, ast)
            emit(lastBuffer, 'end', ast)
            lastBuffer = nextBuffer
        end
    end

    if wrapper == 'iife' then
        local iifeargs = scope.vararg and '...' or ''
        emit(parent, ('local function %s(%s)'):format(tostring(s), iifeargs), ast)
        emit(parent, buffer, ast)
        emit(parent, 'end', ast)
        return expr(('%s(%s)'):format(tostring(s), iifeargs), 'statement')
    elseif wrapper == 'none' then
        -- Splice result right into code
        for i = 1, #buffer do
            emit(parent, buffer[i], ast)
        end
        return {returned = true}
    else -- wrapper == 'target'
        emit(parent, ('local %s'):format(innerTarget), ast)
        for i = 1, #buffer do
            emit(parent, buffer[i], ast)
        end
        return targetExprs
    end
end
docSpecial("if", {"cond1", "body1", "...", "condN", "bodyN"},
           "Conditional form.\n" ..
               "Takes any number of condition/body pairs and evaluates the first body where"
               .. "\nthe condition evaluates to truthy. Similar to cond in other lisps.")

-- (each [k v (pairs t)] body...) => []
SPECIALS["each"] = function(ast, scope, parent)
    local binding = assertCompile(isTable(ast[2]), "expected binding table", ast)
    assertCompile(#ast >= 3, "expected body expression", ast[1])
    local iter = table.remove(binding, #binding) -- last item is iterator call
    local destructures = {}
    local newManglings = {}
    local function destructureBinding(v)
        if isSym(v) then
            return declareLocal(v, {}, scope, ast, newManglings)
        else
            local raw = sym(gensym(scope))
            destructures[raw] = v
            return declareLocal(raw, {}, scope, ast)
        end
    end
    local bindVars = map(binding, destructureBinding)
    local vals = compile1(iter, scope, parent)
    local valNames = map(vals, tostring)

    emit(parent, ('for %s in %s do'):format(table.concat(bindVars, ', '),
                                            table.concat(valNames, ", ")), ast)
    local chunk = {}
    for raw, args in stablepairs(destructures) do
        destructure(args, raw, ast, scope, chunk,
                    { declaration = true, nomulti = true })
    end
    applyManglings(scope, newManglings, ast)
    compileDo(ast, scope, chunk, 3)
    emit(parent, chunk, ast)
    emit(parent, 'end', ast)
end
docSpecial("each", {"[key value (iterator)]", "..."},
           "Runs the body once for each set of values provided by the given iterator."
           .."\nMost commonly used with ipairs for sequential tables or pairs for"
               .." undefined\norder, but can be used with any iterator.")

-- (while condition body...) => []
SPECIALS["while"] = function(ast, scope, parent)
    local len1 = #parent
    local condition = compile1(ast[2], scope, parent, {nval = 1})[1]
    local len2 = #parent
    local subChunk = {}
    if len1 ~= len2 then
        -- Compound condition
        -- Move new compilation to subchunk
        for i = len1 + 1, len2 do
            subChunk[#subChunk + 1] = parent[i]
            parent[i] = nil
        end
        emit(parent, 'while true do', ast)
        emit(subChunk, ('if not %s then break end'):format(condition[1]), ast)
    else
        -- Simple condition
        emit(parent, 'while ' .. tostring(condition) .. ' do', ast)
    end
    compileDo(ast, makeScope(scope), subChunk, 3)
    emit(parent, subChunk, ast)
    emit(parent, 'end', ast)
end
docSpecial("while", {"condition", "..."},
           "The classic while loop. Evaluates body until a condition is non-truthy.")

SPECIALS["for"] = function(ast, scope, parent)
    local ranges = assertCompile(isTable(ast[2]), "expected binding table", ast)
    local bindingSym = table.remove(ast[2], 1)
    assertCompile(isSym(bindingSym),
                  ("unable to bind %s %s"):
                      format(type(bindingSym), tostring(bindingSym)), ast[2])
    assertCompile(#ast >= 3, "expected body expression", ast[1])
    local rangeArgs = {}
    for i = 1, math.min(#ranges, 3) do
        rangeArgs[i] = tostring(compile1(ranges[i], scope, parent, {nval = 1})[1])
    end
    emit(parent, ('for %s = %s do'):format(
             declareLocal(bindingSym, {}, scope, ast),
             table.concat(rangeArgs, ', ')), ast)
    local chunk = {}
    compileDo(ast, scope, chunk, 3)
    emit(parent, chunk, ast)
    emit(parent, 'end', ast)
end
docSpecial("for", {"[index start stop step?]", "..."}, "Numeric loop construct." ..
               "\nEvaluates body once for each value between start and stop (inclusive).")

SPECIALS[":"] = function(ast, scope, parent)
    assertCompile(#ast >= 3, "expected at least 2 arguments", ast)
    -- Compile object
    local objectexpr = compile1(ast[2], scope, parent, {nval = 1})[1]
    -- Compile method selector
    local methodstring
    local methodident = false
    if type(ast[3]) == 'string' and isValidLuaIdentifier(ast[3]) then
        methodident = true
        methodstring = ast[3]
    else
        methodstring = tostring(compile1(ast[3], scope, parent, {nval = 1})[1])
        objectexpr = once(objectexpr, ast[2], scope, parent)
    end
    -- Compile arguments
    local args = {}
    for i = 4, #ast do
        local subexprs = compile1(ast[i], scope, parent, {
            nval = i ~= #ast and 1 or nil
        })
        map(subexprs, tostring, args)
    end
    local fstring
    if not methodident then
        -- Make object first argument
        table.insert(args, 1, tostring(objectexpr))
        fstring = objectexpr.type == 'sym'
            and '%s[%s](%s)'
            or '(%s)[%s](%s)'
    elseif(objectexpr.type == 'literal' or objectexpr.type == 'expression') then
        fstring = '(%s):%s(%s)'
    else
        fstring = '%s:%s(%s)'
    end
    return expr(fstring:format(
        tostring(objectexpr),
        methodstring,
        table.concat(args, ', ')), 'statement')
end
docSpecial(":", {"tbl", "method-name", "..."},
           "Call the named method on tbl with the provided args."..
           "\nMethod name doesn\"t have to be known at compile-time; if it is, use"
               .."\n(tbl:method-name ...) instead.")

SPECIALS["comment"] = function(ast, _, parent)
    local els = {}
    for i = 2, #ast do
        els[#els + 1] = tostring(ast[i]):gsub('\n', ' ')
    end
    emit(parent, '              -- ' .. table.concat(els, ' '), ast)
end
docSpecial("comment", {"..."}, "Comment which will be emitted in Lua output.")

SPECIALS["hashfn"] = function(ast, scope, parent)
    assertCompile(#ast == 2, "expected one argument", ast)
    local fScope = makeScope(scope)
    local fChunk = {}
    local name = gensym(scope)
    local symbol = sym(name)
    declareLocal(symbol, {}, scope, ast)
    fScope.vararg = false
    fScope.hashfn = true
    local args = {}
    for i = 1, 9 do args[i] = declareLocal(sym('$' .. i), {}, fScope, ast) end
    -- Compile body
    compile1(ast[2], fScope, fChunk, {tail = true})
    local maxUsed = 0
    for i = 1, 9 do if fScope.symmeta['$' .. i].used then maxUsed = i end end
    local argStr = table.concat(args, ', ', 1, maxUsed)
    emit(parent, ('local function %s(%s)'):format(name, argStr), ast)
    emit(parent, fChunk, ast)
    emit(parent, 'end', ast)
    return expr(name, 'sym')
end
docSpecial("hashfn", {"..."}, "Function literal shorthand; args are $1, $2, etc.")

local function defineArithmeticSpecial(name, zeroArity, unaryPrefix, luaName)
    local paddedOp = ' ' .. (luaName or name) .. ' '
    SPECIALS[name] = function(ast, scope, parent)
        local len = #ast
        if len == 1 then
            assertCompile(zeroArity ~= nil, 'Expected more than 0 arguments', ast)
            return expr(zeroArity, 'literal')
        else
            local operands = {}
            for i = 2, len do
                local subexprs = compile1(ast[i], scope, parent, {
                    nval = (i == 1 and 1 or nil)
                })
                map(subexprs, tostring, operands)
            end
            if #operands == 1 then
                if unaryPrefix then
                    return '(' .. unaryPrefix .. paddedOp .. operands[1] .. ')'
                else
                    return operands[1]
                end
            else
                return '(' .. table.concat(operands, paddedOp) .. ')'
            end
        end
    end
    docSpecial(name, {"a", "b", "..."},
               "Arithmetic operator; works the same as Lua but accepts more arguments.")
end

defineArithmeticSpecial('+', '0')
defineArithmeticSpecial('..', "''")
defineArithmeticSpecial('^')
defineArithmeticSpecial('-', nil, '')
defineArithmeticSpecial('*', '1')
defineArithmeticSpecial('%')
defineArithmeticSpecial('/', nil, '1')
defineArithmeticSpecial('//', nil, '1')

defineArithmeticSpecial("lshift", nil, "1", "<<")
defineArithmeticSpecial("rshift", nil, "1", ">>")
defineArithmeticSpecial("band", "0", "0", "&")
defineArithmeticSpecial("bor", "0", "0", "|")
defineArithmeticSpecial("bxor", "0", "0", "~")

docSpecial("lshift", {"x", "n"},
           "Bitwise logical left shift of x by n bits; only works in Lua 5.3+.")
docSpecial("rshift", {"x", "n"},
           "Bitwise logical right shift of x by n bits; only works in Lua 5.3+.")
docSpecial("band", {"x1", "x2"}, "Bitwise AND of arguments; only works in Lua 5.3+.")
docSpecial("bor", {"x1", "x2"}, "Bitwise OR of arguments; only works in Lua 5.3+.")
docSpecial("bxor", {"x1", "x2"}, "Bitwise XOR of arguments; only works in Lua 5.3+.")

defineArithmeticSpecial('or', 'false')
defineArithmeticSpecial('and', 'true')

docSpecial("and", {"a", "b", "..."},
           "Boolean operator; works the same as Lua but accepts more arguments.")
docSpecial("or", {"a", "b", "..."},
           "Boolean operator; works the same as Lua but accepts more arguments.")
docSpecial("..", {"a", "b", "..."},
           "String concatenation operator; works the same as Lua but accepts more arguments.")

local function defineComparatorSpecial(name, realop, chainOp)
    local op = realop or name
    SPECIALS[name] = function(ast, scope, parent)
        local len = #ast
        assertCompile(len > 2, "expected at least two arguments", ast)
        local lhs = compile1(ast[2], scope, parent, {nval = 1})[1]
        local lastval = compile1(ast[3], scope, parent, {nval = 1})[1]
        -- avoid double-eval by introducing locals for possible side-effects
        if len > 3 then lastval = once(lastval, ast[3], scope, parent) end
        local out = ('(%s %s %s)'):
            format(tostring(lhs), op, tostring(lastval))
        if len > 3 then
            for i = 4, len do -- variadic comparison
                local nextval = once(compile1(ast[i], scope, parent, {nval = 1})[1],
                                     ast[i], scope, parent)
                out = (out .. " %s (%s %s %s)"):
                    format(chainOp or 'and', tostring(lastval), op, tostring(nextval))
                lastval = nextval
            end
            out = '(' .. out .. ')'
        end
        return out
    end
    docSpecial(name, {"a", "b", "..."},
               "Comparison operator; works the same as Lua but accepts more arguments.")
end

defineComparatorSpecial('>')
defineComparatorSpecial('<')
defineComparatorSpecial('>=')
defineComparatorSpecial('<=')
defineComparatorSpecial('=', '==')
defineComparatorSpecial('not=', '~=', 'or')
SPECIALS["~="] = SPECIALS["not="] -- backwards-compatibility alias

local function defineUnarySpecial(op, realop)
    SPECIALS[op] = function(ast, scope, parent)
        assertCompile(#ast == 2, 'expected one argument', ast)
        local tail = compile1(ast[2], scope, parent, {nval = 1})
        return (realop or op) .. tostring(tail[1])
    end
end

defineUnarySpecial("not", "not ")
docSpecial("not", {"x"}, "Logical operator; works the same as Lua.")

defineUnarySpecial("bnot", "~")
docSpecial("bnot", {"x"}, "Bitwise negation; only works in Lua 5.3+.")

defineUnarySpecial("length", "#")
docSpecial("length", {"x"}, "Returns the length of a table or string.")
SPECIALS["#"] = SPECIALS["length"]

local requireSpecial
local function compile(ast, options)
    local opts = copy(options)
    local oldGlobals = allowedGlobals
    setResetRoot(rootChunk, rootScope, rootOptions)
    allowedGlobals = opts.allowedGlobals
    if opts.indent == nil then opts.indent = '  ' end
    local chunk = {}
    local scope = opts.scope or makeScope(GLOBAL_SCOPE)
    rootChunk, rootScope, rootOptions = chunk, scope, opts
    if opts.requireAsInclude then scope.specials.require = requireSpecial end
    local exprs = compile1(ast, scope, chunk, {tail = true})
    keepSideEffects(exprs, chunk, nil, ast)
    allowedGlobals = oldGlobals
    resetRoot()
    return flatten(chunk, opts)
end

-- make a transformer for key / value table pairs, preserving all numeric keys
local function entryTransform(fk,fv)
    return function(k, v)
        if type(k) == 'number' then
            return k,fv(v)
        else
            return fk(k),fv(v)
        end
    end
end

-- consume everything return nothing
local function no() end

local function mixedConcat(t, joiner)
    local ret = ""
    local s = ""
    local seen = {}
    for k,v in ipairs(t) do
        table.insert(seen, k)
        ret = ret .. s .. v
        s = joiner
    end
    for k,v in stablepairs(t) do
        if not(seen[k]) then
            ret = ret .. s .. '[' .. k .. ']' .. '=' .. v
            s = joiner
        end
    end
    return ret
end

-- expand a quoted form into a data literal, evaluating unquote
local function doQuote (form, scope, parent, runtime)
    local q = function (x) return doQuote(x, scope, parent, runtime) end
    -- vararg
    if isVarg(form) then
        assertCompile(not runtime, "quoted ... may only be used at compile time", form)
        return "_VARARG"
    -- symbol
    elseif isSym(form) then
        assertCompile(not runtime, "symbols may only be used at compile time", form)
        -- We should be able to use "%q" for this but Lua 5.1 throws an error
        -- when you try to format nil, because it's extremely bad.
        local filename = form.filename and ("'%s'"):format(form.filename) or "nil"
        if deref(form):find("#$") then -- autogensym
            return ("sym('%s', nil, {filename=%s, line=%s})"):
                format(autogensym(deref(form), scope), filename, form.line or "nil")
        else -- prevent non-gensymmed symbols from being bound as an identifier
            return ("sym('%s', nil, {quoted=true, filename=%s, line=%s})"):
                format(deref(form), filename, form.line or "nil")
        end
    -- unquote
    elseif isList(form) and isSym(form[1]) and (deref(form[1]) == 'unquote') then
        local payload = form[2]
        local res = unpack(compile1(payload, scope, parent))
        return res[1]
    -- list
    elseif isList(form) then
        assertCompile(not runtime, "lists may only be used at compile time", form)
        local mapped = kvmap(form, entryTransform(no, q))
        local filename = form.filename and ("'%s'"):format(form.filename) or "nil"
        local s = "(function(l) l.filename, l.line = %s, %s return l end)(list(%s))"
        return (s):format(filename, form.line or "nil", mixedConcat(mapped, ", "))
    -- table
    elseif type(form) == 'table' then
        local mapped = kvmap(form, entryTransform(q, q))
        local source = getmetatable(form)
        local filename = source.filename and ("'%s'"):format(source.filename) or "nil"
        return ("setmetatable({%s}, {filename=%s, line=%s})"):
            format(mixedConcat(mapped, ", "), filename, source and source.line or "nil")
    -- string
    elseif type(form) == 'string' then
        return serializeString(form)
    else
        return tostring(form)
    end
end

SPECIALS['quote'] = function(ast, scope, parent)
    assertCompile(#ast == 2, "expected one argument")
    local runtime, thisScope = true, scope
    while thisScope do
        thisScope = thisScope.parent
        if thisScope == COMPILER_SCOPE then runtime = false end
    end
    return doQuote(ast[2], scope, parent, runtime)
end
docSpecial('quote', {'x'}, 'Quasiquote the following form. Only works in macro/compiler scope.')

local function compileStream(strm, options)
    local opts = copy(options)
    local oldGlobals = allowedGlobals
    setResetRoot(rootChunk, rootScope, rootOptions)
    allowedGlobals = opts.allowedGlobals
    if opts.indent == nil then opts.indent = '  ' end
    local scope = opts.scope or makeScope(GLOBAL_SCOPE)
    if opts.requireAsInclude then scope.specials.require = requireSpecial end
    local vals = {}
    for ok, val in parser(strm, opts.filename, opts) do
        if not ok then break end
        vals[#vals + 1] = val
    end
    local chunk = {}
    rootChunk, rootScope, rootOptions = chunk, scope, opts
    for i = 1, #vals do
        local exprs = compile1(vals[i], scope, chunk, {
            tail = i == #vals,
            nval = i < #vals and 0 or nil
        })
        keepSideEffects(exprs, chunk, nil, vals[i])
    end
    allowedGlobals = oldGlobals
    resetRoot()
    return flatten(chunk, opts)
end

local function compileString(str, options)
    local strm = stringStream(str)
    return compileStream(strm, options)
end

---
--- Evaluation
---

-- Convert a fennel environment table to a Lua environment table.
-- This means automatically unmangling globals when getting a value,
-- and mangling values when setting a value. This means the original
-- env will see its values updated as expected, regardless of mangling rules.
local function wrapEnv(env)
    return setmetatable({}, {
        __index = function(_, key)
            if type(key) == 'string' then
                key = globalUnmangling(key)
            end
            return env[key]
        end,
        __newindex = function(_, key, value)
            if type(key) == 'string' then
                key = globalMangling(key)
            end
            env[key] = value
        end,
        -- checking the __pairs metamethod won't work automatically in Lua 5.1
        -- sadly, but it's important for 5.2+ and can be done manually in 5.1
        __pairs = function()
            local function putenv(k, v)
                return type(k) == 'string' and globalUnmangling(k) or k, v
            end
            local pt = kvmap(env, putenv)
            return next, pt, nil
        end,
    })
end

-- A custom traceback function for Fennel that looks similar to
-- the Lua's debug.traceback.
-- Use with xpcall to produce fennel specific stacktraces.
local function traceback(msg, start)
    local level = start or 2 -- Can be used to skip some frames
    local lines = {}
    if msg then
        if msg:find("^Compile error") or msg:find("^Parse error") then
            -- End users don't want to see compiler stack traces, but when
            -- you're hacking on the compiler, export FENNEL_DEBUG=trace
            if not debugOn("trace") then return msg end
            table.insert(lines, msg)
        else
            local newmsg = msg:gsub('^[^:]*:%d+:%s+', 'runtime error: ')
            table.insert(lines, newmsg)
        end
    end
    table.insert(lines, 'stack traceback:')
    while true do
        local info = debug.getinfo(level, "Sln")
        if not info then break end
        local line
        if info.what == "C" then
            if info.name then
                line = ('  [C]: in function \'%s\''):format(info.name)
            else
                line = '  [C]: in ?'
            end
        else
            local remap = fennelSourcemap[info.source]
            if remap and remap[info.currentline] then
                -- And some global info
                info.short_src = remap.short_src
                local mapping = remap[info.currentline]
                -- Overwrite info with values from the mapping (mapping is now
                -- just integer, but may eventually be a table)
                info.currentline = mapping
            end
            if info.what == 'Lua' then
                local n = info.name and ("'" .. info.name .. "'") or '?'
                line = ('  %s:%d: in function %s'):format(info.short_src, info.currentline, n)
            elseif info.short_src == '(tail call)' then
                line = '  (tail call)'
            else
                line = ('  %s:%d: in main chunk'):format(info.short_src, info.currentline)
            end
        end
        table.insert(lines, line)
        level = level + 1
    end
    return table.concat(lines, '\n')
end

local function currentGlobalNames(env)
    return kvmap(env or _G, globalUnmangling)
end

local function eval(str, options, ...)
    local opts = copy(options)
    -- eval and dofile are considered "live" entry points, so we can assume
    -- that the globals available at compile time are a reasonable allowed list
    -- UNLESS there's a metatable on env, in which case we can't assume that
    -- pairs will return all the effective globals; for instance openresty
    -- sets up _G in such a way that all the globals are available thru
    -- the __index meta method, but as far as pairs is concerned it's empty.
    if opts.allowedGlobals == nil and not getmetatable(opts.env) then
        opts.allowedGlobals = currentGlobalNames(opts.env)
    end
    local env = opts.env and wrapEnv(opts.env)
    local luaSource = compileString(str, opts)
    local loader = loadCode(luaSource, env,
                            opts.filename and ('@' .. opts.filename) or str)
    opts.filename = nil
    return loader(...)
end

local function dofileFennel(filename, options, ...)
    local opts = copy(options)
    if opts.allowedGlobals == nil then
        opts.allowedGlobals = currentGlobalNames(opts.env)
    end
    local f = assert(io.open(filename, "rb"))
    local source = f:read("*all"):gsub("^#![^\n]*\n", "")
    f:close()
    opts.filename = filename
    return eval(source, opts, ...)
end

local macroLoaded = {}

local pathTable = {"./?.fnl", "./?/init.fnl"}
table.insert(pathTable, getenv("FENNEL_PATH"))

local module = {
    parser = parser,
    granulate = granulate,
    stringStream = stringStream,
    compile = compile,
    compileString = compileString,
    compileStream = compileStream,
    compile1 = compile1,
    loadCode = loadCode,
    mangle = globalMangling,
    unmangle = globalUnmangling,
    list = list,
    sym = sym,
    varg = varg,
    scope = makeScope,
    gensym = gensym,
    eval = eval,
    dofile = dofileFennel,
    macroLoaded = macroLoaded,
    path = table.concat(pathTable, ";"),
    traceback = traceback,
    version = "0.4.1-dev",
}

-- In order to make this more readable, you can switch your editor to treating
-- this file as if it were Fennel for the purposes of this section
local replsource = [===[(local (fennel internals) ...)

(fn default-read-chunk [parser-state]
  (io.write (if (< 0 parser-state.stackSize) ".." ">> "))
  (io.flush)
  (let [input (io.read)]
    (and input (.. input "\n"))))

(fn default-on-values [xs]
  (io.write (table.concat xs "\t"))
  (io.write "\n"))

(fn default-on-error [errtype err lua-source]
  (io.write
   (match errtype
     "Lua Compile" (.. "Bad code generated - likely a bug with the compiler:\n"
                       "--- Generated Lua Start ---\n"
                       lua-source
                       "--- Generated Lua End ---\n")
     "Runtime" (.. (fennel.traceback err 4) "\n")
     _ (: "%s error: %s\n" :format errtype (tostring err)))))

(local save-source
       (table.concat ["local ___i___ = 1"
                      "while true do"
                      " local name, value = debug.getlocal(1, ___i___)"
                      " if(name and name ~= \"___i___\") then"
                      " ___replLocals___[name] = value"
                      " ___i___ = ___i___ + 1"
                      " else break end end"] "\n"))

(fn splice-save-locals [env lua-source]
  (set env.___replLocals___ (or env.___replLocals___ {}))
  (let [spliced-source []
        bind "local %s = ___replLocals___['%s']"]
    (each [line (lua-source:gmatch "([^\n]+)\n?")]
      (table.insert spliced-source line))
    (each [name (pairs env.___replLocals___)]
      (table.insert spliced-source 1 (bind:format name name)))
    (when (and (< 1 (# spliced-source))
               (: (. spliced-source (# spliced-source)) :match "^ *return .*$"))
      (table.insert spliced-source (# spliced-source) save-source))
    (table.concat spliced-source "\n")))

(fn completer [env scope text]
  (let [matches []
        input-fragment (text:gsub ".*[%s)(]+" "")]
    (fn add-partials [input tbl prefix] ; add partial key matches in tbl
      (each [k (internals.allPairs tbl)]
        (let [k (if (or (= tbl env) (= tbl env.___replLocals___))
                    (. scope.unmanglings k)
                    k)]
          (when (and (< (# matches) 2000) ; stop explosion on too many items
                     (= (type k) "string")
                     (= input (k:sub 0 (# input))))
            (table.insert matches (.. prefix k))))))
    (fn add-matches [input tbl prefix] ; add matches, descending into tbl fields
      (let [prefix (if prefix (.. prefix ".") "")]
        (if (not (input:find "%.")) ; no more dots, so add matches
            (add-partials input tbl prefix)
            (let [(head tail) (input:match "^([^.]+)%.(.*)")
                  raw-head (if (or (= tbl env) (= tbl env.___replLocals___))
                               (. scope.manglings head)
                               head)]
              (when (= (type (. tbl raw-head)) "table")
                (add-matches tail (. tbl raw-head) (.. prefix head)))))))

    (add-matches input-fragment (or scope.specials []))
    (add-matches input-fragment (or scope.macros []))
    (add-matches input-fragment (or env.___replLocals___ []))
    (add-matches input-fragment env)
    (add-matches input-fragment (or env._ENV env._G []))
    matches))

(fn repl [options]
  (let [old-root-options internals.rootOptions
        env (if options.env
                (internals.wrapEnv options.env)
                (setmetatable {} {:__index (or _G._ENV _G)}))
        save-locals? (and (not= options.saveLocals false)
                          env.debug env.debug.getlocal)
        opts {}
        _ (each [k v (pairs options)] (tset opts k v))
        read-chunk (or opts.readChunk default-read-chunk)
        on-values (or opts.onValues default-on-values)
        on-error (or opts.onError default-on-error)
        pp (or opts.pp tostring)
        ;; make parser
        (byte-stream clear-stream) (fennel.granulate read-chunk)
        chars []
        (read reset) (fennel.parser (fn [parser-state]
                                      (let [c (byte-stream parser-state)]
                                        (tset chars (+ (# chars) 1) c)
                                        c)))
        scope (fennel.scope)]

    ;; use metadata unless we've specifically disabled it
    (set opts.useMetadata (not= options.useMetadata false))
    (when (= opts.allowedGlobals nil)
      (set opts.allowedGlobals (internals.currentGlobalNames opts.env)))

    (when opts.registerCompleter
      (opts.registerCompleter (partial completer env scope)))

    (fn loop []
      (each [k (pairs chars)] (tset chars k nil))
      (let [(ok parse-ok? x) (pcall read)
            src-string (string.char ((or _G.unpack table.unpack) chars))]
        (internals.setRootOptions opts)
        (if (not ok)
            (do (on-error "Parse" parse-ok?)
                (clear-stream)
                (reset)
                (loop))
            (when parse-ok? ; if this is false, we got eof
              (match (pcall fennel.compile x {:correlate opts.correlate
                                              :source src-string
                                              :scope scope
                                              :useMetadata opts.useMetadata
                                              :moduleName opts.moduleName
                                              :assert-compile opts.assert-compile
                                              :parse-error opts.parse-error})
                (false msg) (do (clear-stream)
                                (on-error "Compile" msg))
                (true source) (let [source (if save-locals?
                                               (splice-save-locals env source)
                                               source)
                                    (lua-ok? loader) (pcall fennel.loadCode
                                                            source env)]
                                (if (not lua-ok?)
                                    (do (clear-stream)
                                        (on-error "Lua Compile" loader source))
                                    (match (xpcall #[(loader)]
                                                   (partial on-error "Runtime"))
                                      (true ret)
                                      (do (set env._ (. ret 1))
                                          (set env.__ ret)
                                          (on-values (internals.map ret pp)))))))
              (internals.setRootOptions old-root-options)
              (loop)))))
    (loop)))]===]

module.repl = function(options)
    -- functionality the repl needs that isn't part of the public API yet
    local internals = { rootOptions = rootOptions,
                        setRootOptions = function(r) rootOptions = r end,
                        currentGlobalNames = currentGlobalNames,
                        wrapEnv = wrapEnv,
                        allPairs = allPairs,
                        map = map }
    return eval(replsource, { correlate = true }, module, internals)(options)
end

local function searchModule(modulename, pathstring)
    modulename = modulename:gsub("%.", "/")
    for path in string.gmatch((pathstring or module.path)..";", "([^;]*);") do
        local filename = path:gsub("%?", modulename)
        local file = io.open(filename, "rb")
        if(file) then
            file:close()
            return filename
        end
    end
end

module.makeSearcher = function(options)
    return function(modulename)
      -- this will propagate options from the repl but not from eval, because
      -- eval unsets rootOptions after compiling but before running the actual
      -- calls to require.
      local opts = copy(rootOptions)
      for k,v in pairs(options or {}) do opts[k] = v end
      local filename = searchModule(modulename)
      if filename then
         return function(modname)
            return dofileFennel(filename, opts, modname)
         end
      end
   end
end

-- Add metadata and docstrings to fennel module
module.metadata = metadata
module.doc = doc

-- This will allow regular `require` to work with Fennel:
-- table.insert(package.loaders, fennel.searcher)
module.searcher = module.makeSearcher()
module.make_searcher = module.makeSearcher -- oops backwards compatibility

local function makeCompilerEnv(ast, scope, parent)
    return setmetatable({
        -- State of compiler if needed
        _SCOPE = scope,
        _CHUNK = parent,
        _AST = ast,
        _IS_COMPILER = true,
        _SPECIALS = SPECIALS,
        _VARARG = VARARG,
        -- Expose the module in the compiler
        fennel = module,
        -- Useful for macros and meta programming. All of Fennel can be accessed
        -- via fennel.myfun, for example (fennel.eval "(print 1)").
        list = list,
        sym = sym,
        unpack = unpack,
        gensym = function() return sym(gensym(macroCurrentScope or scope)) end,
        ["list?"] = isList,
        ["multi-sym?"] = isMultiSym,
        ["sym?"] = isSym,
        ["table?"] = isTable,
        ["sequence?"] = isSequence,
        ["varg?"] = isVarg,
        ["get-scope"] = function() return macroCurrentScope end,
        ["in-scope?"] = function(symbol)
            assertCompile(macroCurrentScope, "must call from macro", ast)
            return macroCurrentScope.manglings[tostring(symbol)]
        end,
        ["macroexpand"] = function(form)
            assertCompile(macroCurrentScope, "must call from macro", ast)
            return macroexpand(form, macroCurrentScope)
        end,
    }, { __index = _ENV or _G })
end

local function macroGlobals(env, globals)
    local allowed = currentGlobalNames(env)
    for _, k in pairs(globals or {}) do table.insert(allowed, k) end
    return allowed
end

local function addMacros(macros, ast, scope)
    assertCompile(isTable(macros), 'expected macros to be table', ast)
    for k,v in pairs(macros) do
        assertCompile(type(v) == 'function', 'expected each macro to be function', ast)
        scope.macros[k] = v
    end
end

local function loadMacros(modname, ast, scope, parent)
    local filename = assertCompile(searchModule(modname),
                                   modname .. " module not found.", ast)
    local env = makeCompilerEnv(ast, scope, parent)
    local globals = macroGlobals(env, currentGlobalNames())
    return dofileFennel(filename, { env = env, allowedGlobals = globals,
                                    useMetadata = rootOptions.useMetadata,
                                    scope = COMPILER_SCOPE })
end

SPECIALS['require-macros'] = function(ast, scope, parent)
    assertCompile(#ast == 2, "Expected one module name argument", ast)
    local modname = ast[2]
    if not macroLoaded[modname] then
        macroLoaded[modname] = loadMacros(modname, ast, scope, parent)
    end
    addMacros(macroLoaded[modname], ast, scope, parent)
end
docSpecial('require-macros', {'macro-module-name'},
           'Load given module and use its contents as macro definitions in current scope.'
               ..'\nMacro module should return a table of macro functions with string keys.'
               ..'\nConsider using import-macros instead as it is more flexible.')

SPECIALS['include'] = function(ast, scope, parent, opts)
    assertCompile(#ast == 2, 'expected one argument', ast)

    -- Compile mod argument
    local modexpr = compile1(ast[2], scope, parent, {nval = 1})[1]
    if modexpr.type ~= 'literal' or modexpr[1]:byte() ~= 34 then
        if opts.fallback then
            return opts.fallback(modexpr)
        else
            assertCompile(false, 'module name must resolve to a string literal', ast)
        end
    end
    local code = 'return ' .. modexpr[1]
    local mod = loadCode(code)()

    -- Check cache
    if rootScope.includes[mod] then return rootScope.includes[mod] end

    -- Find path to source
    local path = searchModule(mod)
    local isFennel = true
    if not path then
        isFennel = false
        path = searchModule(mod, package.path)
        if not path then
            if opts.fallback then
                return opts.fallback(modexpr)
            else
                assertCompile(false, 'module not found ' .. mod, ast)
            end
        end
    end

    -- Read source
    local f = io.open(path)
    local s = f:read('*all')
    f:close()

    -- splice in source and memoize it in compiler AND package.preload
    -- so we can include it again without duplication, even in runtime
    local target = 'package.preload["' .. mod .. '"]'
    local ret = expr('require("' .. mod .. '")', 'statement')

    local subChunk, tempChunk = {}, {}
    emit(tempChunk, subChunk, ast)
    -- if lua, simply emit the setting of package.preload
    if not isFennel then
        emit(tempChunk, target .. ' = ' .. target .. ' or function()\n' .. s .. '\nend', ast)
    end
    -- Splice tempChunk to begining of rootChunk
    for i, v in ipairs(tempChunk) do
        table.insert(rootChunk, i, v)
    end

    -- For fnl source, compile subChunk AFTER splicing into start of rootChunk.
    if isFennel then
        local subopts = { nval = 1, target = target }
        local subscope = makeScope(rootScope.parent)
        if rootOptions.requireAsInclude then
            subscope.specials.require = requireSpecial
        end
        local targetForm = list(sym('.'), sym('package.preload'), mod)
        -- splice "or" statement in so it uses existing package.preload[modname]
        -- if it's been set by something else, allowing for overrides
        local forms = list(sym('or'), targetForm, list(sym('fn'), sequence()))
        local p = parser(stringStream(s), path)
        for _, val in p do table.insert(forms[3], val) end
        compile1(forms, subscope, subChunk, subopts)
    end

    -- Put in cache and return
    rootScope.includes[mod] = ret
    return ret
end
docSpecial('include', {'module-name-literal'},
           'Like require, but load the target module during compilation and embed it in the\n'
        .. 'Lua output. The module must be a string literal and resolvable at compile time.')

local function requireFallback(e)
    local code = ('require(%s)'):format(tostring(e))
    return expr(code, 'statement')
end

requireSpecial = function (ast, scope, parent, opts)
    opts.fallback = requireFallback
    return SPECIALS['include'](ast, scope, parent, opts)
end

local function evalCompiler(ast, scope, parent)
    local luaSource = compile(ast, { scope = makeScope(COMPILER_SCOPE),
                                     useMetadata = rootOptions.useMetadata })
    local loader = loadCode(luaSource, wrapEnv(makeCompilerEnv(ast, scope, parent)))
    return loader()
end

SPECIALS['macros'] = function(ast, scope, parent)
    assertCompile(#ast == 2, "Expected one table argument", ast)
    local macros = evalCompiler(ast[2], scope, parent)
    addMacros(macros, ast, scope, parent)
end
docSpecial('macros', {'{:macro-name-1 (fn [...] ...) ... :macro-name-N macro-body-N}'},
           'Define all functions in the given table as macros local to the current scope.')

SPECIALS['eval-compiler'] = function(ast, scope, parent)
    local oldFirst = ast[1]
    ast[1] = sym('do')
    local val = evalCompiler(ast, scope, parent)
    ast[1] = oldFirst
    return val
end
docSpecial('eval-compiler', {'...'}, 'Evaluate the body at compile-time.'
               .. ' Use the macro system instead if possible.')

-- Load standard macros
local stdmacros = [===[
{"->" (fn [val ...]
        "Thread-first macro.
Take the first value and splice it into the second form as its first argument.
The value of the second form is spliced into the first arg of the third, etc."
        (var x val)
        (each [_ e (ipairs [...])]
          (let [elt (if (list? e) e (list e))]
            (table.insert elt 2 x)
            (set x elt)))
        x)
 "->>" (fn [val ...]
         "Thread-last macro.
Same as ->, except splices the value into the last position of each form
rather than the first."
         (var x val)
         (each [_ e (pairs [...])]
           (let [elt (if (list? e) e (list e))]
             (table.insert elt x)
             (set x elt)))
         x)
 "-?>" (fn [val ...]
         "Nil-safe thread-first macro.
Same as -> except will short-circuit with nil when it encounters a nil value."
         (if (= 0 (select "#" ...))
             val
             (let [els [...]
                   e (table.remove els 1)
                   el (if (list? e) e (list e))
                   tmp (gensym)]
               (table.insert el 2 tmp)
               `(let [,tmp ,val]
                  (if ,tmp
                      (-?> ,el ,(unpack els))
                      ,tmp)))))
 "-?>>" (fn [val ...]
         "Nil-safe thread-last macro.
Same as ->> except will short-circuit with nil when it encounters a nil value."
          (if (= 0 (select "#" ...))
              val
              (let [els [...]
                    e (table.remove els 1)
                    el (if (list? e) e (list e))
                    tmp (gensym)]
                (table.insert el tmp)
                `(let [,tmp ,val]
                   (if ,tmp
                       (-?>> ,el ,(unpack els))
                       ,tmp)))))
 :doto (fn [val ...]
         "Evaluates val and splices it into the first argument of subsequent forms."
         (let [name (gensym)
               form `(let [,name ,val])]
           (each [_ elt (pairs [...])]
             (table.insert elt 2 name)
             (table.insert form elt))
           (table.insert form name)
           form))
 :when (fn [condition body1 ...]
         "Evaluate body for side-effects only when condition is truthy."
         (assert body1 "expected body")
         `(if ,condition
              (do ,body1 ,...)))
 :partial (fn [f ...]
            "Returns a function with all arguments partially applied to f."
            (let [body (list f ...)]
              (table.insert body _VARARG)
              `(fn [,_VARARG] ,body)))
 :pick-args (fn [n f]
               "Creates a function of arity n that applies its arguments to f.
For example,\n\t(pick-args 2 func)
expands to\n\t(fn [_0_ _1_] (func _0_ _1_))"
               (assert (and (= (type n) :number) (= n (math.floor n)) (>= n 0))
                 "Expected n to be an integer literal >= 0.")
               (let [bindings []]
                 (for [i 1 n] (tset bindings i (gensym)))
                 `(fn ,bindings (,f ,(unpack bindings)))))
 :pick-values (fn [n ...]
                 "Like the `values` special, but emits exactly n values.\nFor example,
\t(pick-values 2 ...)\nexpands to\n\t(let [(_0_ _1_) ...] (values _0_ _1_))"
                 (assert (and (= :number (type n)) (>= n 0) (= n (math.floor n)))
                         "Expected n to be an integer >= 0")
                 (let [let-syms   (list)
                       let-values (if (= 1 (select :# ...)) ... `(values ,...))]
                   (for [i 1 n] (table.insert let-syms (gensym)))
                   (if (= n 0) `(values)
                       `(let [,let-syms ,let-values] (values ,(unpack let-syms))))))
 :lambda (fn [...]
           "Function literal with arity checking.
Will throw an exception if a declared argument is passed in as nil, unless
that argument name begins with ?."
           (let [args [...]
                 has-internal-name? (sym? (. args 1))
                 arglist (if has-internal-name? (. args 2) (. args 1))
                 docstring-position (if has-internal-name? 3 2)
                 has-docstring? (and (> (# args) docstring-position)
                                     (= :string (type (. args docstring-position))))
                 arity-check-position (- 4 (if has-internal-name? 0 1) (if has-docstring? 0 1))]
             (fn check! [a]
               (if (table? a)
                   (each [_ a (pairs a)]
                     (check! a))
                   (and (not (: (tostring a) :match "^?"))
                        (not= (tostring a) "&")
                        (not= (tostring a) "..."))
                   (table.insert args arity-check-position
                                 `(assert (not= nil ,a)
                                          (: "Missing argument %s on %s:%s"
                                             :format ,(tostring a)
                                             ,(or a.filename "unknown")
                                             ,(or a.line "?"))))))
             (assert (> (length args) 1) "expected body expression")
             (each [_ a (ipairs arglist)]
               (check! a))
             `(fn ,(unpack args))))
 :macro (fn macro [name ...]
          "Define a single macro."
          (assert (sym? name) "expected symbol for macro name")
          (local args [...])
          `(macros { ,(tostring name) (fn ,name ,(unpack args))}))
 :macrodebug (fn macrodebug [form]
              "Print the resulting form after performing macroexpansion."
              (let [(ok view) (pcall require :fennelview)]
                `(print ,((if ok view tostring)
                          (macroexpand form _SCOPE)))))
 :import-macros (fn import-macros [binding1 module-name1 ...]
                  "Binds a table of macros from each macro module according to its binding form.
Each binding form can be either a symbol or a k/v destructuring table.
Example:\n  (import-macros mymacros                 :my-macros    ; bind to symbol
                 {:macro1 alias : macro2} :proj.macros) ; import by name"
                  (assert (and binding1 module-name1 (= 0 (% (select :# ...) 2)))
                          "expected even number of binding/modulename pairs")
                  (for [i 1 (select :# binding1 module-name1 ...) 2]
                    (local (binding modname) (select i binding1 module-name1 ...))
                    ;; generate a subscope of current scope, use require-macros to bring in macro
                    ;; module. after that, we just copy the macros from subscope to scope.
                    (local scope (get-scope))
                    (local subscope (fennel.scope scope))
                    (fennel.compileString (string.format "(require-macros %q)" modname)
                                          {:scope subscope})
                    (if (sym? binding)
                      ;; bind whole table of macros to table bound to symbol
                      (do (tset scope.macros (. binding 1) {})
                          (each [k v (pairs subscope.macros)]
                            (tset (. scope.macros (. binding 1)) k v)))

                      ;; 1-level table destructuring for importing individual macros
                      (table? binding)
                      (each [macro-name [import-key] (pairs binding)]
                        (assert (= :function (type (. subscope.macros macro-name)))
                                (.. "macro " macro-name " not found in module " modname))
                        (tset scope.macros import-key (. subscope.macros macro-name)))))
                  ;; TODO: replace with `nil` once we fix macros being able to return nil
                  `(do nil))
 :match
(fn match [val ...]
  "Perform pattern matching on val. See reference for details."
  ;; this function takes the AST of values and a single pattern and returns a
  ;; condition to determine if it matches as well as a list of bindings to
  ;; introduce for the duration of the body if it does match.
  (fn match-pattern [vals pattern unifications]
    ;; we have to assume we're matching against multiple values here until we
    ;; know we're either in a multi-valued clause (in which case we know the #
    ;; of vals) or we're not, in which case we only care about the first one.
    (let [[val] vals]
      (if (or (and (sym? pattern) ; unification with outer locals (or nil)
                   (not= :_ (tostring pattern)) ; never unify _
                   (or (in-scope? pattern)
                       (= :nil (tostring pattern))))
              (and (multi-sym? pattern)
                   (in-scope? (. (multi-sym? pattern) 1))))
          (values `(= ,val ,pattern) [])
          ;; unify a local we've seen already
          (and (sym? pattern)
               (. unifications (tostring pattern)))
          (values `(= ,(. unifications (tostring pattern)) ,val) [])
          ;; bind a fresh local
          (sym? pattern)
          (let [wildcard? (= (tostring pattern) "_")]
            (if (not wildcard?) (tset unifications (tostring pattern) val))
            (values (if (or wildcard? (: (tostring pattern) :find "^?"))
                        true `(not= ,(sym :nil) ,val))
                    [pattern val]))
          ;; guard clause
          (and (list? pattern) (sym? (. pattern 2)) (= :? (tostring (. pattern 2))))
          (let [(pcondition bindings) (match-pattern vals (. pattern 1)
                                                     unifications)
                condition `(and ,pcondition)]
            (for [i 3 (# pattern)] ; splice in guard clauses
              (table.insert condition (. pattern i)))
            (values `(let ,bindings ,condition) bindings))

          ;; multi-valued patterns (represented as lists)
          (list? pattern)
          (let [condition `(and)
                bindings []]
            (each [i pat (ipairs pattern)]
              (let [(subcondition subbindings) (match-pattern [(. vals i)] pat
                                                              unifications)]
                (table.insert condition subcondition)
                (each [_ b (ipairs subbindings)]
                  (table.insert bindings b))))
            (values condition bindings))
          ;; table patterns)
          (= (type pattern) :table)
          (let [condition `(and (= (type ,val) :table))
                bindings []]
            (each [k pat (pairs pattern)]
              (if (and (sym? pat) (= "&" (tostring pat)))
                  (do (assert (not (. pattern (+ k 2)))
                              "expected rest argument before last parameter")
                      (table.insert bindings (. pattern (+ k 1)))
                      (table.insert bindings [`(select ,k ((or _G.unpack table.unpack)
                                                           ,val))]))
                  (and (= :number (type k))
                       (= "&" (tostring (. pattern (- k 1)))))
                  nil ; don't process the pattern right after &; already got it
                  (let [subval `(. ,val ,k)
                        (subcondition subbindings) (match-pattern [subval] pat
                                                                  unifications)]
                    (table.insert condition subcondition)
                    (each [_ b (ipairs subbindings)]
                      (table.insert bindings b)))))
            (values condition bindings))
          ;; literal value
          (values `(= ,val ,pattern) []))))
  (fn match-condition [vals clauses]
    (let [out `(if)]
      (for [i 1 (length clauses) 2]
        (let [pattern (. clauses i)
              body (. clauses (+ i 1))
              (condition bindings) (match-pattern vals pattern {})]
          (table.insert out condition)
          (table.insert out `(let ,bindings ,body))))
      out))
  ;; how many multi-valued clauses are there? return a list of that many gensyms
  (fn val-syms [clauses]
    (let [syms (list (gensym))]
      (for [i 1 (length clauses) 2]
        (if (list? (. clauses i))
            (each [valnum (ipairs (. clauses i))]
              (if (not (. syms valnum))
                  (tset syms valnum (gensym))))))
      syms))
  ;; wrap it in a way that prevents double-evaluation of the matched value
  (let [clauses [...]
        vals (val-syms clauses)]
    (if (not= 0 (% (length clauses) 2)) ; treat odd final clause as default
        (table.insert clauses (length clauses) (sym :_)))
    ;; protect against multiple evaluation of the value, bind against as
    ;; many values as we ever match against in the clauses.
    (list (sym :let) [vals val]
          (match-condition vals clauses))))
 }
]===]
do
    -- docstrings rely on having a place to "put" metadata; we use the module
    -- system for that. but if you try to require the module while it's being
    -- loaded, you get a stack overflow. so we fake out the module for the
    -- purposes of boostrapping the built-in macros here.
    local moduleName = "__fennel-bootstrap__"
    package.preload[moduleName] = function() package.loaded['fennel'] = module end
    local env = makeCompilerEnv(nil, COMPILER_SCOPE, {})
    local macros = eval(stdmacros, {
                            env = env,
                            scope = makeScope(COMPILER_SCOPE),
                            -- assume the code to load globals doesn't have any
                            -- mistaken globals, otherwise this can be
                            -- problematic when loading fennel in contexts
                            -- where _G is an empty table with an __index
                            -- metamethod. (openresty)
                            allowedGlobals = false,
                            useMetadata = true,
                            filename = "built-ins",
                            moduleName = moduleName })
    for k,v in pairs(macros) do GLOBAL_SCOPE.macros[k] = v end
    package.preload[moduleName] = nil
end
GLOBAL_SCOPE.macros['λ'] = GLOBAL_SCOPE.macros['lambda']

package.loaded['fennel'] = module
package.preload.fennelfriend = function()
local function ast_source(ast)
  local m = getmetatable(ast)
  if (m and m.filename and m.line and m) then
    return m
  else
    return ast
  end
end
local suggestions = {["can't start multisym segment with a digit"] = {"removing the digit", "adding a non-digit before the digit"}, ["cannot call literal value"] = {"checking for typos", "checking for a missing function name"}, ["could not compile value of type "] = {"debugging the macro you're calling not to return a coroutine or userdata"}, ["could not read number (.*)"] = {"removing the non-digit character", "beginning the identifier with a non-digit if it is not meant to be a number"}, ["expected a function.* to call"] = {"removing the empty parentheses", "using square brackets if you want an empty table"}, ["expected binding table"] = {"placing a table here in square brackets containing identifiers to bind"}, ["expected body expression"] = {"putting some code in the body of this form after the bindings"}, ["expected each macro to be function"] = {"ensuring that the value for each key in your macros table contains a function", "avoid defining nested macro tables"}, ["expected even number of name/value bindings"] = {"finding where the identifier or value is missing"}, ["expected even number of values in table literal"] = {"removing a key", "adding a value"}, ["expected local"] = {"looking for a typo", "looking for a local which is used out of its scope"}, ["expected macros to be table"] = {"ensuring your macro definitions return a table"}, ["expected parameters"] = {"adding function parameters as a list of identifiers in brackets"}, ["expected rest argument before last parameter"] = {"moving & to right before the final identifier when destructuring"}, ["expected symbol for function parameter: (.*)"] = {"changing %s to an identifier instead of a literal value"}, ["expected var (.*)"] = {"declaring %s using var instead of let/local", "introducing a new local instead of changing the value of %s"}, ["expected vararg as last parameter"] = {"moving the \"...\" to the end of the parameter list"}, ["expected whitespace before opening delimiter"] = {"adding whitespace"}, ["global (.*) conflicts with local"] = {"renaming local %s"}, ["illegal character: (.)"] = {"deleting or replacing %s", "avoiding reserved characters like \", \\, ', ~, ;, @, `, and comma"}, ["local (.*) was overshadowed by a special form or macro"] = {"renaming local %s"}, ["macro not found in macro module"] = {"checking the keys of the imported macro module's returned table"}, ["macro tried to bind (.*) without gensym"] = {"changing to %s# when introducing identifiers inside macros"}, ["malformed multisym"] = {"ensuring each period or colon is not followed by another period or colon"}, ["may only be used at compile time"] = {"moving this to inside a macro if you need to manipulate symbols/lists", "using square brackets instead of parens to construct a table"}, ["method must be last component"] = {"using a period instead of a colon for field access", "removing segments after the colon", "making the method call, then looking up the field on the result"}, ["mismatched closing delimiter (.), expected (.)"] = {"replacing %s with %s", "deleting %s", "adding matching opening delimiter earlier"}, ["multisym method calls may only be in call position"] = {"using a period instead of a colon to reference a table's fields", "putting parens around this"}, ["unable to bind (.*)"] = {"replacing the %s with an identifier"}, ["unexpected closing delimiter (.)"] = {"deleting %s", "adding matching opening delimiter earlier"}, ["unexpected multi symbol (.*)"] = {"removing periods or colons from %s"}, ["unexpected vararg"] = {"putting \"...\" at the end of the fn parameters if the vararg was intended"}, ["unknown global in strict mode: (.*)"] = {"looking to see if there's a typo", "using the _G table instead, eg. _G.%s if you really want a global", "moving this code to somewhere that %s is in scope", "binding %s as a local in the scope of this code"}, ["unused local (.*)"] = {"fixing a typo so %s is used", "renaming the local to _%s"}, ["use of global (.*) is aliased by a local"] = {"renaming local %s"}}
local unpack = (_G.unpack or table.unpack)
local function suggest(msg)
  local suggestion = nil
  for pat, sug in pairs(suggestions) do
    local matches = {msg:match(pat)}
    if (0 < #matches) then
      if ("table" == type(sug)) then
        local out = {}
        for _, s in ipairs(sug) do
          table.insert(out, s:format(unpack(matches)))
        end
        suggestion = out
      else
        suggestion = sug(matches)
      end
    end
  end
  return suggestion
end
local function read_line_from_file(filename, line)
  local bytes = 0
  local f = assert(io.open(filename))
  local _ = nil
  for _0 = 1, (line - 1) do
    bytes = (bytes + 1 + #f:read())
  end
  _ = nil
  local codeline = f:read()
  local eol = (bytes + #codeline)
  f:close()
  return codeline, bytes, eol
end
local function friendly_msg(msg, _0_0)
  local _1_ = _0_0
  local byteend = _1_["byteend"]
  local bytestart = _1_["bytestart"]
  local filename = _1_["filename"]
  local line = _1_["line"]
  local ok, codeline, bol, eol = pcall(read_line_from_file, filename, line)
  local suggestions0 = suggest(msg)
  local out = {msg, ""}
  if (ok and codeline) then
    table.insert(out, codeline)
  end
  if (ok and codeline and bytestart and byteend) then
    table.insert(out, (string.rep(" ", (bytestart - bol - 1)) .. "^" .. string.rep("^", math.min((byteend - bytestart), (eol - bytestart)))))
  end
  if (ok and codeline and bytestart and not byteend) then
    table.insert(out, (string.rep("-", (bytestart - bol - 1)) .. "^"))
    table.insert(out, "")
  end
  if suggestions0 then
    for _, suggestion in ipairs(suggestions0) do
      table.insert(out, ("* Try %s."):format(suggestion))
    end
  end
  return table.concat(out, "\n")
end
local function assert_compile(condition, msg, ast)
  if not condition then
    local _1_ = ast_source(ast)
    local filename = _1_["filename"]
    local line = _1_["line"]
    error(friendly_msg(("Compile error in %s:%s\n  %s"):format((filename or "unknown"), (line or "?"), msg), ast_source(ast)), 0)
  end
  return condition
end
local function parse_error(msg, filename, line, bytestart)
  return error(friendly_msg(("Parse error in %s:%s\n  %s"):format(filename, line, msg), {bytestart = bytestart, filename = filename, line = line}), 0)
end
return {["assert-compile"] = assert_compile, ["parse-error"] = parse_error}
end
