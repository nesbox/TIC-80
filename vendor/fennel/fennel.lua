-- SPDX-License-Identifier: MIT
-- SPDX-FileCopyrightText: Calvin Rose and contributors
package.preload["fennel.repl"] = package.preload["fennel.repl"] or function(...)
  local utils = require("fennel.utils")
  local parser = require("fennel.parser")
  local compiler = require("fennel.compiler")
  local specials = require("fennel.specials")
  local view = require("fennel.view")
  local depth = 0
  local function prompt_for(top_3f)
    if top_3f then
      return (string.rep(">", (depth + 1)) .. " ")
    else
      return (string.rep(".", (depth + 1)) .. " ")
    end
  end
  local function default_read_chunk(parser_state)
    io.write(prompt_for((0 == parser_state["stack-size"])))
    io.flush()
    local input = io.read()
    return (input and (input .. "\n"))
  end
  local function default_on_values(xs)
    io.write(table.concat(xs, "\9"))
    return io.write("\n")
  end
  local function default_on_error(errtype, err)
    local function _675_()
      local _674_0 = errtype
      if (_674_0 == "Runtime") then
        return (compiler.traceback(tostring(err), 4) .. "\n")
      else
        local _ = _674_0
        return ("%s error: %s\n"):format(errtype, tostring(err))
      end
    end
    return io.write(_675_())
  end
  local function splice_save_locals(env, lua_source, scope)
    local saves = nil
    do
      local tbl_17_ = {}
      local i_18_ = #tbl_17_
      for name in pairs(env.___replLocals___) do
        local val_19_ = ("local %s = ___replLocals___[%q]"):format((scope.manglings[name] or name), name)
        if (nil ~= val_19_) then
          i_18_ = (i_18_ + 1)
          tbl_17_[i_18_] = val_19_
        end
      end
      saves = tbl_17_
    end
    local binds = nil
    do
      local tbl_17_ = {}
      local i_18_ = #tbl_17_
      for raw, name in pairs(scope.manglings) do
        local val_19_ = nil
        if not scope.gensyms[name] then
          val_19_ = ("___replLocals___[%q] = %s"):format(raw, name)
        else
        val_19_ = nil
        end
        if (nil ~= val_19_) then
          i_18_ = (i_18_ + 1)
          tbl_17_[i_18_] = val_19_
        end
      end
      binds = tbl_17_
    end
    local gap = nil
    if lua_source:find("\n") then
      gap = "\n"
    else
      gap = " "
    end
    local function _681_()
      if next(saves) then
        return (table.concat(saves, " ") .. gap)
      else
        return ""
      end
    end
    local function _684_()
      local _682_0, _683_0 = lua_source:match("^(.*)[\n ](return .*)$")
      if ((nil ~= _682_0) and (nil ~= _683_0)) then
        local body = _682_0
        local _return = _683_0
        return (body .. gap .. table.concat(binds, " ") .. gap .. _return)
      else
        local _ = _682_0
        return lua_source
      end
    end
    return (_681_() .. _684_())
  end
  local commands = {}
  local function completer(env, scope, text, _3ffulltext, _from, _to)
    local max_items = 2000
    local seen = {}
    local matches = {}
    local input_fragment = text:gsub(".*[%s)(]+", "")
    local stop_looking_3f = false
    local function add_partials(input, tbl, prefix)
      local scope_first_3f = ((tbl == env) or (tbl == env.___replLocals___))
      local tbl_17_ = matches
      local i_18_ = #tbl_17_
      local function _686_()
        if scope_first_3f then
          return scope.manglings
        else
          return tbl
        end
      end
      for k, is_mangled in utils.allpairs(_686_()) do
        if (max_items <= #matches) then break end
        local val_19_ = nil
        do
          local lookup_k = nil
          if scope_first_3f then
            lookup_k = is_mangled
          else
            lookup_k = k
          end
          if ((type(k) == "string") and (input == k:sub(0, #input)) and not seen[k] and ((":" ~= prefix:sub(-1)) or ("function" == type(tbl[lookup_k])))) then
            seen[k] = true
            val_19_ = (prefix .. k)
          else
          val_19_ = nil
          end
        end
        if (nil ~= val_19_) then
          i_18_ = (i_18_ + 1)
          tbl_17_[i_18_] = val_19_
        end
      end
      return tbl_17_
    end
    local function descend(input, tbl, prefix, add_matches, method_3f)
      local splitter = nil
      if method_3f then
        splitter = "^([^:]+):(.*)"
      else
        splitter = "^([^.]+)%.(.*)"
      end
      local head, tail = input:match(splitter)
      local raw_head = (scope.manglings[head] or head)
      if (type(tbl[raw_head]) == "table") then
        stop_looking_3f = true
        if method_3f then
          return add_partials(tail, tbl[raw_head], (prefix .. head .. ":"))
        else
          return add_matches(tail, tbl[raw_head], (prefix .. head))
        end
      end
    end
    local function add_matches(input, tbl, prefix)
      local prefix0 = nil
      if prefix then
        prefix0 = (prefix .. ".")
      else
        prefix0 = ""
      end
      if (not input:find("%.") and input:find(":")) then
        return descend(input, tbl, prefix0, add_matches, true)
      elseif not input:find("%.") then
        return add_partials(input, tbl, prefix0)
      else
        return descend(input, tbl, prefix0, add_matches, false)
      end
    end
    do
      local _695_0 = tostring((_3ffulltext or text)):match("^%s*,([^%s()[%]]*)$")
      if (nil ~= _695_0) then
        local cmd_fragment = _695_0
        add_partials(cmd_fragment, commands, ",")
      else
        local _ = _695_0
        for _0, source in ipairs({scope.specials, scope.macros, (env.___replLocals___ or {}), env, env._G}) do
          if stop_looking_3f then break end
          add_matches(input_fragment, source)
        end
      end
    end
    return matches
  end
  local function command_3f(input)
    return input:match("^%s*,")
  end
  local function command_docs()
    local _697_
    do
      local tbl_17_ = {}
      local i_18_ = #tbl_17_
      for name, f in utils.stablepairs(commands) do
        local val_19_ = ("  ,%s - %s"):format(name, ((compiler.metadata):get(f, "fnl/docstring") or "undocumented"))
        if (nil ~= val_19_) then
          i_18_ = (i_18_ + 1)
          tbl_17_[i_18_] = val_19_
        end
      end
      _697_ = tbl_17_
    end
    return table.concat(_697_, "\n")
  end
  commands.help = function(_, _0, on_values)
    return on_values({("Welcome to Fennel.\nThis is the REPL where you can enter code to be evaluated.\nYou can also run these repl commands:\n\n" .. command_docs() .. "\n  ,return FORM - Evaluate FORM and return its value to the REPL's caller.\n  ,exit - Leave the repl.\n\nUse ,doc something to see descriptions for individual macros and special forms.\nValues from previous inputs are kept in *1, *2, and *3.\n\nFor more information about the language, see https://fennel-lang.org/reference")})
  end
  do end (compiler.metadata):set(commands.help, "fnl/docstring", "Show this message.")
  local function reload(module_name, env, on_values, on_error)
    local _699_0, _700_0 = pcall(specials["load-code"]("return require(...)", env), module_name)
    if ((_699_0 == true) and (nil ~= _700_0)) then
      local old = _700_0
      local _ = nil
      package.loaded[module_name] = nil
      _ = nil
      local new = nil
      do
        local _701_0, _702_0 = pcall(require, module_name)
        if ((_701_0 == true) and (nil ~= _702_0)) then
          local new0 = _702_0
          new = new0
        elseif (true and (nil ~= _702_0)) then
          local _0 = _701_0
          local msg = _702_0
          on_error("Repl", msg)
          new = old
        else
        new = nil
        end
      end
      specials["macro-loaded"][module_name] = nil
      if ((type(old) == "table") and (type(new) == "table")) then
        for k, v in pairs(new) do
          old[k] = v
        end
        for k in pairs(old) do
          if (nil == new[k]) then
            old[k] = nil
          end
        end
        package.loaded[module_name] = old
      end
      return on_values({"ok"})
    elseif ((_699_0 == false) and (nil ~= _700_0)) then
      local msg = _700_0
      if msg:match("loop or previous error loading module") then
        package.loaded[module_name] = nil
        return reload(module_name, env, on_values, on_error)
      elseif specials["macro-loaded"][module_name] then
        specials["macro-loaded"][module_name] = nil
        return nil
      else
        local function _707_()
          local _706_0 = msg:gsub("\n.*", "")
          return _706_0
        end
        return on_error("Runtime", _707_())
      end
    end
  end
  local function run_command(read, on_error, f)
    local _710_0, _711_0, _712_0 = pcall(read)
    if ((_710_0 == true) and (_711_0 == true) and (nil ~= _712_0)) then
      local val = _712_0
      local _713_0, _714_0 = pcall(f, val)
      if ((_713_0 == false) and (nil ~= _714_0)) then
        local msg = _714_0
        return on_error("Runtime", msg)
      end
    elseif (_710_0 == false) then
      return on_error("Parse", "Couldn't parse input.")
    end
  end
  commands.reload = function(env, read, on_values, on_error)
    local function _717_(_241)
      return reload(tostring(_241), env, on_values, on_error)
    end
    return run_command(read, on_error, _717_)
  end
  do end (compiler.metadata):set(commands.reload, "fnl/docstring", "Reload the specified module.")
  commands.reset = function(env, _, on_values)
    env.___replLocals___ = {}
    return on_values({"ok"})
  end
  do end (compiler.metadata):set(commands.reset, "fnl/docstring", "Erase all repl-local scope.")
  commands.complete = function(env, read, on_values, on_error, scope, chars)
    local function _718_()
      return on_values(completer(env, scope, table.concat(chars):gsub("^%s*,complete%s+", ""):sub(1, -2)))
    end
    return run_command(read, on_error, _718_)
  end
  do end (compiler.metadata):set(commands.complete, "fnl/docstring", "Print all possible completions for a given input symbol.")
  local function apropos_2a(pattern, tbl, prefix, seen, names)
    for name, subtbl in pairs(tbl) do
      if (("string" == type(name)) and (package ~= subtbl)) then
        local _719_0 = type(subtbl)
        if (_719_0 == "function") then
          if ((prefix .. name)):match(pattern) then
            table.insert(names, (prefix .. name))
          end
        elseif (_719_0 == "table") then
          if not seen[subtbl] then
            local _721_
            do
              seen[subtbl] = true
              _721_ = seen
            end
            apropos_2a(pattern, subtbl, (prefix .. name:gsub("%.", "/") .. "."), _721_, names)
          end
        end
      end
    end
    return names
  end
  local function apropos(pattern)
    return apropos_2a(pattern:gsub("^_G%.", ""), package.loaded, "", {}, {})
  end
  commands.apropos = function(_env, read, on_values, on_error, _scope)
    local function _725_(_241)
      return on_values(apropos(tostring(_241)))
    end
    return run_command(read, on_error, _725_)
  end
  do end (compiler.metadata):set(commands.apropos, "fnl/docstring", "Print all functions matching a pattern in all loaded modules.")
  local function apropos_follow_path(path)
    local paths = nil
    do
      local tbl_17_ = {}
      local i_18_ = #tbl_17_
      for p in path:gmatch("[^%.]+") do
        local val_19_ = p
        if (nil ~= val_19_) then
          i_18_ = (i_18_ + 1)
          tbl_17_[i_18_] = val_19_
        end
      end
      paths = tbl_17_
    end
    local tgt = package.loaded
    for _, path0 in ipairs(paths) do
      if (nil == tgt) then break end
      local _728_
      do
        local _727_0 = path0:gsub("%/", ".")
        _728_ = _727_0
      end
      tgt = tgt[_728_]
    end
    return tgt
  end
  local function apropos_doc(pattern)
    local tbl_17_ = {}
    local i_18_ = #tbl_17_
    for _, path in ipairs(apropos(".*")) do
      local val_19_ = nil
      do
        local tgt = apropos_follow_path(path)
        if ("function" == type(tgt)) then
          local _729_0 = (compiler.metadata):get(tgt, "fnl/docstring")
          if (nil ~= _729_0) then
            local docstr = _729_0
            val_19_ = (docstr:match(pattern) and path)
          else
          val_19_ = nil
          end
        else
        val_19_ = nil
        end
      end
      if (nil ~= val_19_) then
        i_18_ = (i_18_ + 1)
        tbl_17_[i_18_] = val_19_
      end
    end
    return tbl_17_
  end
  commands["apropos-doc"] = function(_env, read, on_values, on_error, _scope)
    local function _733_(_241)
      return on_values(apropos_doc(tostring(_241)))
    end
    return run_command(read, on_error, _733_)
  end
  do end (compiler.metadata):set(commands["apropos-doc"], "fnl/docstring", "Print all functions that match the pattern in their docs")
  local function apropos_show_docs(on_values, pattern)
    for _, path in ipairs(apropos(pattern)) do
      local tgt = apropos_follow_path(path)
      if (("function" == type(tgt)) and (compiler.metadata):get(tgt, "fnl/docstring")) then
        on_values({specials.doc(tgt, path)})
        on_values({})
      end
    end
    return nil
  end
  commands["apropos-show-docs"] = function(_env, read, on_values, on_error)
    local function _735_(_241)
      return apropos_show_docs(on_values, tostring(_241))
    end
    return run_command(read, on_error, _735_)
  end
  do end (compiler.metadata):set(commands["apropos-show-docs"], "fnl/docstring", "Print all documentations matching a pattern in function name")
  local function resolve(identifier, _736_0, scope)
    local _737_ = _736_0
    local env = _737_
    local ___replLocals___ = _737_["___replLocals___"]
    local e = nil
    local function _738_(_241, _242)
      return (___replLocals___[scope.unmanglings[_242]] or env[_242])
    end
    e = setmetatable({}, {__index = _738_})
    local function _739_(...)
      local _740_0, _741_0 = ...
      if ((_740_0 == true) and (nil ~= _741_0)) then
        local code = _741_0
        local function _742_(...)
          local _743_0, _744_0 = ...
          if ((_743_0 == true) and (nil ~= _744_0)) then
            local val = _744_0
            return val
          else
            local _ = _743_0
            return nil
          end
        end
        return _742_(pcall(specials["load-code"](code, e)))
      else
        local _ = _740_0
        return nil
      end
    end
    return _739_(pcall(compiler["compile-string"], tostring(identifier), {scope = scope}))
  end
  commands.find = function(env, read, on_values, on_error, scope)
    local function _747_(_241)
      local _748_0 = nil
      do
        local _749_0 = utils["sym?"](_241)
        if (nil ~= _749_0) then
          local _750_0 = resolve(_749_0, env, scope)
          if (nil ~= _750_0) then
            _748_0 = debug.getinfo(_750_0)
          else
            _748_0 = _750_0
          end
        else
          _748_0 = _749_0
        end
      end
      if ((_G.type(_748_0) == "table") and (nil ~= _748_0.linedefined) and (nil ~= _748_0.short_src) and (nil ~= _748_0.source) and (_748_0.what == "Lua")) then
        local line = _748_0.linedefined
        local src = _748_0.short_src
        local source = _748_0.source
        local fnlsrc = nil
        do
          local _753_0 = compiler.sourcemap
          if (nil ~= _753_0) then
            _753_0 = _753_0[source]
          end
          if (nil ~= _753_0) then
            _753_0 = _753_0[line]
          end
          if (nil ~= _753_0) then
            _753_0 = _753_0[2]
          end
          fnlsrc = _753_0
        end
        return on_values({string.format("%s:%s", src, (fnlsrc or line))})
      elseif (_748_0 == nil) then
        return on_error("Repl", "Unknown value")
      else
        local _ = _748_0
        return on_error("Repl", "No source info")
      end
    end
    return run_command(read, on_error, _747_)
  end
  do end (compiler.metadata):set(commands.find, "fnl/docstring", "Print the filename and line number for a given function")
  commands.doc = function(env, read, on_values, on_error, scope)
    local function _758_(_241)
      local name = tostring(_241)
      local path = (utils["multi-sym?"](name) or {name})
      local ok_3f, target = nil, nil
      local function _759_()
        return (scope.specials[name] or utils["get-in"](scope.macros, path) or resolve(name, env, scope))
      end
      ok_3f, target = pcall(_759_)
      if ok_3f then
        return on_values({specials.doc(target, name)})
      else
        return on_error("Repl", ("Could not find " .. name .. " for docs."))
      end
    end
    return run_command(read, on_error, _758_)
  end
  do end (compiler.metadata):set(commands.doc, "fnl/docstring", "Print the docstring and arglist for a function, macro, or special form.")
  commands.compile = function(_, read, on_values, on_error, _0, _1, opts)
    local function _761_(_241)
      local _762_0, _763_0 = pcall(compiler.compile, _241, opts)
      if ((_762_0 == true) and (nil ~= _763_0)) then
        local result = _763_0
        return on_values({result})
      elseif (true and (nil ~= _763_0)) then
        local _2 = _762_0
        local msg = _763_0
        return on_error("Repl", ("Error compiling expression: " .. msg))
      end
    end
    return run_command(read, on_error, _761_)
  end
  do end (compiler.metadata):set(commands.compile, "fnl/docstring", "compiles the expression into lua and prints the result.")
  local function load_plugin_commands(plugins)
    for i = #(plugins or {}), 1, -1 do
      for name, f in pairs(plugins[i]) do
        local _765_0 = name:match("^repl%-command%-(.*)")
        if (nil ~= _765_0) then
          local cmd_name = _765_0
          commands[cmd_name] = f
        end
      end
    end
    return nil
  end
  local function run_command_loop(input, read, loop, env, on_values, on_error, scope, chars, opts)
    local command_name = input:match(",([^%s/]+)")
    do
      local _767_0 = commands[command_name]
      if (nil ~= _767_0) then
        local command = _767_0
        command(env, read, on_values, on_error, scope, chars, opts)
      else
        local _ = _767_0
        if ((command_name ~= "exit") and (command_name ~= "return")) then
          on_values({"Unknown command", command_name})
        end
      end
    end
    if ("exit" ~= command_name) then
      return loop((command_name == "return"))
    end
  end
  local function try_readline_21(opts, ok, readline)
    if ok then
      if readline.set_readline_name then
        readline.set_readline_name("fennel")
      end
      readline.set_options({histfile = "", keeplines = 1000})
      opts.readChunk = function(parser_state)
        local prompt = nil
        if (0 < parser_state["stack-size"]) then
          prompt = ".. "
        else
          prompt = ">> "
        end
        local str = readline.readline(prompt)
        if str then
          return (str .. "\n")
        end
      end
      local completer0 = nil
      opts.registerCompleter = function(repl_completer)
        completer0 = repl_completer
        return nil
      end
      local function repl_completer(text, from, to)
        if completer0 then
          readline.set_completion_append_character("")
          return completer0(text:sub(from, to), text, from, to)
        else
          return {}
        end
      end
      readline.set_complete_function(repl_completer)
      return readline
    end
  end
  local function should_use_readline_3f(opts)
    return (("dumb" ~= os.getenv("TERM")) and not opts.readChunk and not opts.registerCompleter)
  end
  local function repl(_3foptions)
    local old_root_options = utils.root.options
    local _776_ = utils.copy(_3foptions)
    local opts = _776_
    local _3ffennelrc = _776_["fennelrc"]
    local _ = nil
    opts.fennelrc = nil
    _ = nil
    local readline = (should_use_readline_3f(opts) and try_readline_21(opts, pcall(require, "readline")))
    local _0 = nil
    if _3ffennelrc then
      _0 = _3ffennelrc()
    else
    _0 = nil
    end
    local env = specials["wrap-env"]((opts.env or rawget(_G, "_ENV") or _G))
    local callbacks = {["view-opts"] = (opts["view-opts"] or {depth = 4}), env = env, onError = (opts.onError or default_on_error), onValues = (opts.onValues or default_on_values), pp = (opts.pp or view), readChunk = (opts.readChunk or default_read_chunk)}
    local save_locals_3f = (opts.saveLocals ~= false)
    local byte_stream, clear_stream = nil, nil
    local function _778_(_241)
      return callbacks.readChunk(_241)
    end
    byte_stream, clear_stream = parser.granulate(_778_)
    local chars = {}
    local read, reset = nil, nil
    local function _779_(parser_state)
      local b = byte_stream(parser_state)
      if b then
        table.insert(chars, string.char(b))
      end
      return b
    end
    read, reset = parser.parser(_779_)
    depth = (depth + 1)
    if opts.message then
      callbacks.onValues({opts.message})
    end
    env.___repl___ = callbacks
    opts.env, opts.scope = env, compiler["make-scope"]()
    opts.useMetadata = (opts.useMetadata ~= false)
    if (opts.allowedGlobals == nil) then
      opts.allowedGlobals = specials["current-global-names"](env)
    end
    if opts.init then
      opts.init(opts, depth)
    end
    if opts.registerCompleter then
      local function _785_()
        local _784_0 = opts.scope
        local function _786_(...)
          return completer(env, _784_0, ...)
        end
        return _786_
      end
      opts.registerCompleter(_785_())
    end
    load_plugin_commands(opts.plugins)
    if save_locals_3f then
      local function newindex(t, k, v)
        if opts.scope.manglings[k] then
          return rawset(t, k, v)
        end
      end
      env.___replLocals___ = setmetatable({}, {__newindex = newindex})
    end
    local function print_values(...)
      local vals = {...}
      local out = {}
      local pp = callbacks.pp
      env._, env.__ = vals[1], vals
      for i = 1, select("#", ...) do
        table.insert(out, pp(vals[i], callbacks["view-opts"]))
      end
      return callbacks.onValues(out)
    end
    local function save_value(...)
      env.___replLocals___["*3"] = env.___replLocals___["*2"]
      env.___replLocals___["*2"] = env.___replLocals___["*1"]
      env.___replLocals___["*1"] = ...
      return ...
    end
    opts.scope.manglings["*1"], opts.scope.unmanglings._1 = "_1", "*1"
    opts.scope.manglings["*2"], opts.scope.unmanglings._2 = "_2", "*2"
    opts.scope.manglings["*3"], opts.scope.unmanglings._3 = "_3", "*3"
    local function loop(exit_next_3f)
      for k in pairs(chars) do
        chars[k] = nil
      end
      reset()
      local ok, parser_not_eof_3f, form = pcall(read)
      local src_string = table.concat(chars)
      local readline_not_eof_3f = (not readline or (src_string ~= "(null)"))
      local not_eof_3f = (readline_not_eof_3f and parser_not_eof_3f)
      if not ok then
        callbacks.onError("Parse", not_eof_3f)
        clear_stream()
        return loop()
      elseif command_3f(src_string) then
        return run_command_loop(src_string, read, loop, env, callbacks.onValues, callbacks.onError, opts.scope, chars, opts)
      else
        if not_eof_3f then
          local function _790_(...)
            local _791_0, _792_0 = ...
            if ((_791_0 == true) and (nil ~= _792_0)) then
              local src = _792_0
              local function _793_(...)
                local _794_0, _795_0 = ...
                if ((_794_0 == true) and (nil ~= _795_0)) then
                  local chunk = _795_0
                  local function _796_()
                    return print_values(save_value(chunk()))
                  end
                  local function _797_(...)
                    return callbacks.onError("Runtime", ...)
                  end
                  return xpcall(_796_, _797_)
                elseif ((_794_0 == false) and (nil ~= _795_0)) then
                  local msg = _795_0
                  clear_stream()
                  return callbacks.onError("Compile", msg)
                end
              end
              local function _800_(...)
                local src0 = nil
                if save_locals_3f then
                  src0 = splice_save_locals(env, src, opts.scope)
                else
                  src0 = src
                end
                return pcall(specials["load-code"], src0, env)
              end
              return _793_(_800_(...))
            elseif ((_791_0 == false) and (nil ~= _792_0)) then
              local msg = _792_0
              clear_stream()
              return callbacks.onError("Compile", msg)
            end
          end
          local function _802_()
            opts["source"] = src_string
            return opts
          end
          _790_(pcall(compiler.compile, form, _802_()))
          utils.root.options = old_root_options
          if exit_next_3f then
            return env.___replLocals___["*1"]
          else
            return loop()
          end
        end
      end
    end
    local value = loop()
    depth = (depth - 1)
    if readline then
      readline.save_history()
    end
    if opts.exit then
      opts.exit(opts, depth)
    end
    return value
  end
  local function _808_(overrides, _3fopts)
    return repl(utils.copy(_3fopts, utils.copy(overrides)))
  end
  return setmetatable({}, {__call = _808_, __index = {repl = repl}})
end
package.preload["fennel.specials"] = package.preload["fennel.specials"] or function(...)
  local utils = require("fennel.utils")
  local view = require("fennel.view")
  local parser = require("fennel.parser")
  local compiler = require("fennel.compiler")
  local unpack = (table.unpack or _G.unpack)
  local SPECIALS = compiler.scopes.global.specials
  local function str1(x)
    return tostring(x[1])
  end
  local function wrap_env(env)
    local function _449_(_, key)
      if utils["string?"](key) then
        return env[compiler["global-unmangling"](key)]
      else
        return env[key]
      end
    end
    local function _451_(_, key, value)
      if utils["string?"](key) then
        env[compiler["global-unmangling"](key)] = value
        return nil
      else
        env[key] = value
        return nil
      end
    end
    local function _453_()
      local _454_
      do
        local tbl_14_ = {}
        for k, v in utils.stablepairs(env) do
          local k_15_, v_16_ = nil, nil
          local _455_
          if utils["string?"](k) then
            _455_ = compiler["global-unmangling"](k)
          else
            _455_ = k
          end
          k_15_, v_16_ = _455_, v
          if ((k_15_ ~= nil) and (v_16_ ~= nil)) then
            tbl_14_[k_15_] = v_16_
          end
        end
        _454_ = tbl_14_
      end
      return next, _454_, nil
    end
    return setmetatable({}, {__index = _449_, __newindex = _451_, __pairs = _453_})
  end
  local function fennel_module_name()
    return (utils.root.options.moduleName or "fennel")
  end
  local function current_global_names(_3fenv)
    local mt = nil
    do
      local _458_0 = getmetatable(_3fenv)
      if ((_G.type(_458_0) == "table") and (nil ~= _458_0.__pairs)) then
        local mtpairs = _458_0.__pairs
        local tbl_14_ = {}
        for k, v in mtpairs(_3fenv) do
          local k_15_, v_16_ = k, v
          if ((k_15_ ~= nil) and (v_16_ ~= nil)) then
            tbl_14_[k_15_] = v_16_
          end
        end
        mt = tbl_14_
      elseif (_458_0 == nil) then
        mt = (_3fenv or _G)
      else
      mt = nil
      end
    end
    local function _461_()
      local tbl_17_ = {}
      local i_18_ = #tbl_17_
      for k, v in utils.stablepairs(mt) do
        local val_19_ = compiler["global-unmangling"](k)
        if (nil ~= val_19_) then
          i_18_ = (i_18_ + 1)
          tbl_17_[i_18_] = val_19_
        end
      end
      return tbl_17_
    end
    return (mt and _461_())
  end
  local function load_code(code, _3fenv, _3ffilename)
    local env = (_3fenv or rawget(_G, "_ENV") or _G)
    local _463_0, _464_0 = rawget(_G, "setfenv"), rawget(_G, "loadstring")
    if ((nil ~= _463_0) and (nil ~= _464_0)) then
      local setfenv = _463_0
      local loadstring = _464_0
      local f = assert(loadstring(code, _3ffilename))
      setfenv(f, env)
      return f
    else
      local _ = _463_0
      return assert(load(code, _3ffilename, "t", env))
    end
  end
  local function doc_2a(tgt, name)
    if not tgt then
      return (name .. " not found")
    else
      local docstring = (((compiler.metadata):get(tgt, "fnl/docstring") or "#<undocumented>")):gsub("\n$", ""):gsub("\n", "\n  ")
      local mt = getmetatable(tgt)
      if ((type(tgt) == "function") or ((type(mt) == "table") and (type(mt.__call) == "function"))) then
        local elts = nil
        do
          local _466_0 = ((compiler.metadata):get(tgt, "fnl/arglist") or {"#<unknown-arguments>"})
          table.insert(_466_0, 1, name)
          elts = _466_0
        end
        return string.format("(%s)\n  %s", table.concat(elts, " "), docstring)
      else
        return string.format("%s\n  %s", name, docstring)
      end
    end
  end
  local function doc_special(name, arglist, docstring, body_form_3f)
    compiler.metadata[SPECIALS[name]] = {["fnl/arglist"] = arglist, ["fnl/body-form?"] = body_form_3f, ["fnl/docstring"] = docstring}
    return nil
  end
  local function compile_do(ast, scope, parent, _3fstart)
    local start = (_3fstart or 2)
    local len = #ast
    local sub_scope = compiler["make-scope"](scope)
    for i = start, len do
      compiler.compile1(ast[i], sub_scope, parent, {nval = 0})
    end
    return nil
  end
  SPECIALS["do"] = function(ast, scope, parent, opts, _3fstart, _3fchunk, _3fsub_scope, _3fpre_syms)
    local start = (_3fstart or 2)
    local sub_scope = (_3fsub_scope or compiler["make-scope"](scope))
    local chunk = (_3fchunk or {})
    local len = #ast
    local retexprs = {returned = true}
    utils.hook("pre-do", ast, sub_scope)
    local function compile_body(outer_target, outer_tail, outer_retexprs)
      for i = start, len do
        local subopts = {nval = (((i ~= len) and 0) or opts.nval), tail = (((i == len) and outer_tail) or nil), target = (((i == len) and outer_target) or nil)}
        local _ = utils["propagate-options"](opts, subopts)
        local subexprs = compiler.compile1(ast[i], sub_scope, chunk, subopts)
        if (i ~= len) then
          compiler["keep-side-effects"](subexprs, parent, nil, ast[i])
        end
      end
      compiler.emit(parent, chunk, ast)
      compiler.emit(parent, "end", ast)
      utils.hook("do", ast, sub_scope)
      return (outer_retexprs or retexprs)
    end
    if (opts.target or (opts.nval == 0) or opts.tail) then
      compiler.emit(parent, "do", ast)
      return compile_body(opts.target, opts.tail)
    elseif opts.nval then
      local syms = {}
      for i = 1, opts.nval do
        local s = ((_3fpre_syms and _3fpre_syms[i]) or compiler.gensym(scope))
        syms[i] = s
        retexprs[i] = utils.expr(s, "sym")
      end
      local outer_target = table.concat(syms, ", ")
      compiler.emit(parent, string.format("local %s", outer_target), ast)
      compiler.emit(parent, "do", ast)
      return compile_body(outer_target, opts.tail)
    else
      local fname = compiler.gensym(scope)
      local fargs = nil
      if scope.vararg then
        fargs = "..."
      else
        fargs = ""
      end
      compiler.emit(parent, string.format("local function %s(%s)", fname, fargs), ast)
      return compile_body(nil, true, utils.expr((fname .. "(" .. fargs .. ")"), "statement"))
    end
  end
  doc_special("do", {"..."}, "Evaluate multiple forms; return last value.", true)
  local function iter_args(ast)
    local ast0, len, i = ast, #ast, 1
    local function _472_()
      i = (1 + i)
      while ((i == len) and utils["call-of?"](ast0[i], "values")) do
        ast0 = ast0[i]
        len = #ast0
        i = 2
      end
      return ast0[i], (nil == ast0[(i + 1)])
    end
    return _472_
  end
  SPECIALS.values = function(ast, scope, parent)
    local exprs = {}
    for subast, last_3f in iter_args(ast) do
      local subexprs = compiler.compile1(subast, scope, parent, {nval = (not last_3f and 1)})
      table.insert(exprs, subexprs[1])
      if last_3f then
        for j = 2, #subexprs do
          table.insert(exprs, subexprs[j])
        end
      end
    end
    return exprs
  end
  doc_special("values", {"..."}, "Return multiple values from a function. Must be in tail position.")
  local function __3estack(stack, tbl)
    for k, v in pairs(tbl) do
      table.insert(stack, k)
      table.insert(stack, v)
    end
    return stack
  end
  local function literal_3f(val)
    local res = true
    if utils["list?"](val) then
      res = false
    elseif utils["table?"](val) then
      local stack = __3estack({}, val)
      for _, elt in ipairs(stack) do
        if not res then break end
        if utils["list?"](elt) then
          res = false
        elseif utils["table?"](elt) then
          __3estack(stack, elt)
        end
      end
    end
    return res
  end
  local function compile_value(v)
    local opts = {nval = 1, tail = false}
    local scope = compiler["make-scope"]()
    local chunk = {}
    local _476_ = compiler.compile1(v, scope, chunk, opts)
    local _477_ = _476_[1]
    local v0 = _477_[1]
    return v0
  end
  local function insert_meta(meta, k, v)
    local view_opts = {["escape-newlines?"] = true, ["line-length"] = math.huge, ["one-line?"] = true}
    compiler.assert((type(k) == "string"), ("expected string keys in metadata table, got: %s"):format(view(k, view_opts)))
    compiler.assert(literal_3f(v), ("expected literal value in metadata table, got: %s %s"):format(view(k, view_opts), view(v, view_opts)))
    table.insert(meta, view(k))
    local function _478_()
      if ("string" == type(v)) then
        return view(v, view_opts)
      else
        return compile_value(v)
      end
    end
    table.insert(meta, _478_())
    return meta
  end
  local function insert_arglist(meta, arg_list)
    local opts = {["escape-newlines?"] = true, ["line-length"] = math.huge, ["one-line?"] = true}
    local view_args = nil
    do
      local tbl_17_ = {}
      local i_18_ = #tbl_17_
      for _, arg in ipairs(arg_list) do
        local val_19_ = view(view(arg, opts))
        if (nil ~= val_19_) then
          i_18_ = (i_18_ + 1)
          tbl_17_[i_18_] = val_19_
        end
      end
      view_args = tbl_17_
    end
    table.insert(meta, "\"fnl/arglist\"")
    table.insert(meta, ("{" .. table.concat(view_args, ", ") .. "}"))
    return meta
  end
  local function set_fn_metadata(f_metadata, parent, fn_name)
    if utils.root.options.useMetadata then
      local meta_fields = {}
      for k, v in utils.stablepairs(f_metadata) do
        if (k == "fnl/arglist") then
          insert_arglist(meta_fields, v)
        else
          insert_meta(meta_fields, k, v)
        end
      end
      local meta_str = ("require(\"%s\").metadata"):format(fennel_module_name())
      return compiler.emit(parent, ("pcall(function() %s:setall(%s, %s) end)"):format(meta_str, fn_name, table.concat(meta_fields, ", ")))
    end
  end
  local function get_fn_name(ast, scope, fn_name, multi)
    if (fn_name and (fn_name[1] ~= "nil")) then
      local _482_
      if not multi then
        _482_ = compiler["declare-local"](fn_name, scope, ast)
      else
        _482_ = compiler["symbol-to-expression"](fn_name, scope)[1]
      end
      return _482_, not multi, 3
    else
      return nil, true, 2
    end
  end
  local function compile_named_fn(ast, f_scope, f_chunk, parent, index, fn_name, local_3f, arg_name_list, f_metadata)
    utils.hook("pre-fn", ast, f_scope)
    for i = (index + 1), #ast do
      compiler.compile1(ast[i], f_scope, f_chunk, {nval = (((i ~= #ast) and 0) or nil), tail = (i == #ast)})
    end
    local _485_
    if local_3f then
      _485_ = "local function %s(%s)"
    else
      _485_ = "%s = function(%s)"
    end
    compiler.emit(parent, string.format(_485_, fn_name, table.concat(arg_name_list, ", ")), ast)
    compiler.emit(parent, f_chunk, ast)
    compiler.emit(parent, "end", ast)
    set_fn_metadata(f_metadata, parent, fn_name)
    utils.hook("fn", ast, f_scope)
    return utils.expr(fn_name, "sym")
  end
  local function compile_anonymous_fn(ast, f_scope, f_chunk, parent, index, arg_name_list, f_metadata, scope)
    local fn_name = compiler.gensym(scope)
    return compile_named_fn(ast, f_scope, f_chunk, parent, index, fn_name, true, arg_name_list, f_metadata)
  end
  local function maybe_metadata(ast, pred, handler, mt, index)
    local index_2a = (index + 1)
    local index_2a_before_ast_end_3f = (index_2a < #ast)
    local expr = ast[index_2a]
    if (index_2a_before_ast_end_3f and pred(expr)) then
      return handler(mt, expr), index_2a
    else
      return mt, index
    end
  end
  local function get_function_metadata(ast, arg_list, index)
    local function _488_(_241, _242)
      local tbl_14_ = _241
      for k, v in pairs(_242) do
        local k_15_, v_16_ = k, v
        if ((k_15_ ~= nil) and (v_16_ ~= nil)) then
          tbl_14_[k_15_] = v_16_
        end
      end
      return tbl_14_
    end
    local function _490_(_241, _242)
      _241["fnl/docstring"] = _242
      return _241
    end
    return maybe_metadata(ast, utils["kv-table?"], _488_, maybe_metadata(ast, utils["string?"], _490_, {["fnl/arglist"] = arg_list}, index))
  end
  SPECIALS.fn = function(ast, scope, parent, opts)
    local f_scope = nil
    do
      local _491_0 = compiler["make-scope"](scope)
      _491_0["vararg"] = false
      f_scope = _491_0
    end
    local f_chunk = {}
    local fn_sym = utils["sym?"](ast[2])
    local multi = (fn_sym and utils["multi-sym?"](fn_sym[1]))
    local fn_name, local_3f, index = get_fn_name(ast, scope, fn_sym, multi, opts)
    local arg_list = compiler.assert(utils["table?"](ast[index]), "expected parameters table", ast)
    compiler.assert((not multi or not multi["multi-sym-method-call"]), ("unexpected multi symbol " .. tostring(fn_name)), fn_sym)
    if (multi and not scope.symmeta[multi[1]] and not compiler["global-allowed?"](multi[1])) then
      compiler.assert(nil, ("expected local table " .. multi[1]), ast[2])
    end
    local function destructure_arg(arg)
      local raw = utils.sym(compiler.gensym(scope))
      local declared = compiler["declare-local"](raw, f_scope, ast)
      compiler.destructure(arg, raw, ast, f_scope, f_chunk, {declaration = true, nomulti = true, symtype = "arg"})
      return declared
    end
    local function destructure_amp(i)
      compiler.assert((i == (#arg_list - 1)), "expected rest argument before last parameter", arg_list[(i + 1)], arg_list)
      f_scope.vararg = true
      compiler.destructure(arg_list[#arg_list], {utils.varg()}, ast, f_scope, f_chunk, {declaration = true, nomulti = true, symtype = "arg"})
      return "..."
    end
    local function get_arg_name(arg, i)
      if f_scope.vararg then
        return nil
      elseif utils["varg?"](arg) then
        compiler.assert((arg == arg_list[#arg_list]), "expected vararg as last parameter", ast)
        f_scope.vararg = true
        return "..."
      elseif utils["sym?"](arg, "&") then
        return destructure_amp(i)
      elseif (utils["sym?"](arg) and (tostring(arg) ~= "nil") and not utils["multi-sym?"](tostring(arg))) then
        return compiler["declare-local"](arg, f_scope, ast)
      elseif utils["table?"](arg) then
        return destructure_arg(arg)
      else
        return compiler.assert(false, ("expected symbol for function parameter: %s"):format(tostring(arg)), ast[index])
      end
    end
    local arg_name_list = nil
    do
      local tbl_17_ = {}
      local i_18_ = #tbl_17_
      for i, a in ipairs(arg_list) do
        local val_19_ = get_arg_name(a, i)
        if (nil ~= val_19_) then
          i_18_ = (i_18_ + 1)
          tbl_17_[i_18_] = val_19_
        end
      end
      arg_name_list = tbl_17_
    end
    local f_metadata, index0 = get_function_metadata(ast, arg_list, index)
    if fn_name then
      return compile_named_fn(ast, f_scope, f_chunk, parent, index0, fn_name, local_3f, arg_name_list, f_metadata)
    else
      return compile_anonymous_fn(ast, f_scope, f_chunk, parent, index0, arg_name_list, f_metadata, scope)
    end
  end
  doc_special("fn", {"name?", "args", "docstring?", "..."}, "Function syntax. May optionally include a name and docstring or a metadata table.\nIf a name is provided, the function will be bound in the current scope.\nWhen called with the wrong number of args, excess args will be discarded\nand lacking args will be nil, use lambda for arity-checked functions.", true)
  SPECIALS.lua = function(ast, _, parent)
    compiler.assert(((#ast == 2) or (#ast == 3)), "expected 1 or 2 arguments", ast)
    local _497_
    do
      local _496_0 = utils["sym?"](ast[2])
      if (nil ~= _496_0) then
        _497_ = tostring(_496_0)
      else
        _497_ = _496_0
      end
    end
    if ("nil" ~= _497_) then
      table.insert(parent, {ast = ast, leaf = tostring(ast[2])})
    end
    local _501_
    do
      local _500_0 = utils["sym?"](ast[3])
      if (nil ~= _500_0) then
        _501_ = tostring(_500_0)
      else
        _501_ = _500_0
      end
    end
    if ("nil" ~= _501_) then
      return tostring(ast[3])
    end
  end
  local function dot(ast, scope, parent)
    compiler.assert((1 < #ast), "expected table argument", ast)
    local len = #ast
    local lhs_node = compiler.macroexpand(ast[2], scope)
    local _504_ = compiler.compile1(lhs_node, scope, parent, {nval = 1})
    local lhs = _504_[1]
    if (len == 2) then
      return tostring(lhs)
    else
      local indices = {}
      for i = 3, len do
        local index = ast[i]
        if (utils["string?"](index) and utils["valid-lua-identifier?"](index)) then
          table.insert(indices, ("." .. index))
        else
          local _505_ = compiler.compile1(index, scope, parent, {nval = 1})
          local index0 = _505_[1]
          table.insert(indices, ("[" .. tostring(index0) .. "]"))
        end
      end
      if (not (utils["sym?"](lhs_node) or utils["list?"](lhs_node)) or ("nil" == tostring(lhs_node))) then
        return ("(" .. tostring(lhs) .. ")" .. table.concat(indices))
      else
        return (tostring(lhs) .. table.concat(indices))
      end
    end
  end
  SPECIALS["."] = dot
  doc_special(".", {"tbl", "key1", "..."}, "Look up key1 in tbl table. If more args are provided, do a nested lookup.")
  SPECIALS.global = function(ast, scope, parent)
    compiler.assert((#ast == 3), "expected name and value", ast)
    compiler.destructure(ast[2], ast[3], ast, scope, parent, {forceglobal = true, nomulti = true, symtype = "global"})
    return nil
  end
  doc_special("global", {"name", "val"}, "Set name as a global with val. Deprecated.")
  SPECIALS.set = function(ast, scope, parent)
    compiler.assert((#ast == 3), "expected name and value", ast)
    compiler.destructure(ast[2], ast[3], ast, scope, parent, {noundef = true, symtype = "set"})
    return nil
  end
  doc_special("set", {"name", "val"}, "Set a local variable to a new value. Only works on locals using var.")
  local function set_forcibly_21_2a(ast, scope, parent)
    compiler.assert((#ast == 3), "expected name and value", ast)
    compiler.destructure(ast[2], ast[3], ast, scope, parent, {forceset = true, symtype = "set"})
    return nil
  end
  SPECIALS["set-forcibly!"] = set_forcibly_21_2a
  local function local_2a(ast, scope, parent, opts)
    compiler.assert(((0 == opts.nval) or opts.tail), "can't introduce local here", ast)
    compiler.assert((#ast == 3), "expected name and value", ast)
    compiler.destructure(ast[2], ast[3], ast, scope, parent, {declaration = true, nomulti = true, symtype = "local"})
    return nil
  end
  SPECIALS["local"] = local_2a
  doc_special("local", {"name", "val"}, "Introduce new top-level immutable local.")
  SPECIALS.var = function(ast, scope, parent, opts)
    compiler.assert(((0 == opts.nval) or opts.tail), "can't introduce var here", ast)
    compiler.assert((#ast == 3), "expected name and value", ast)
    compiler.destructure(ast[2], ast[3], ast, scope, parent, {declaration = true, isvar = true, nomulti = true, symtype = "var"})
    return nil
  end
  doc_special("var", {"name", "val"}, "Introduce new mutable local.")
  local function kv_3f(t)
    local _509_
    do
      local tbl_17_ = {}
      local i_18_ = #tbl_17_
      for k in pairs(t) do
        local val_19_ = nil
        if ("number" ~= type(k)) then
          val_19_ = k
        else
        val_19_ = nil
        end
        if (nil ~= val_19_) then
          i_18_ = (i_18_ + 1)
          tbl_17_[i_18_] = val_19_
        end
      end
      _509_ = tbl_17_
    end
    return _509_[1]
  end
  SPECIALS.let = function(_512_0, scope, parent, opts)
    local _513_ = _512_0
    local _ = _513_[1]
    local bindings = _513_[2]
    local ast = _513_
    compiler.assert((utils["table?"](bindings) and not kv_3f(bindings)), "expected binding sequence", (bindings or ast[1]))
    compiler.assert(((#bindings % 2) == 0), "expected even number of name/value bindings", bindings)
    compiler.assert((3 <= #ast), "expected body expression", ast[1])
    local pre_syms = nil
    do
      local tbl_17_ = {}
      local i_18_ = #tbl_17_
      for _0 = 1, (opts.nval or 0) do
        local val_19_ = compiler.gensym(scope)
        if (nil ~= val_19_) then
          i_18_ = (i_18_ + 1)
          tbl_17_[i_18_] = val_19_
        end
      end
      pre_syms = tbl_17_
    end
    local sub_scope = compiler["make-scope"](scope)
    local sub_chunk = {}
    for i = 1, #bindings, 2 do
      compiler.destructure(bindings[i], bindings[(i + 1)], ast, sub_scope, sub_chunk, {declaration = true, nomulti = true, symtype = "let"})
    end
    return SPECIALS["do"](ast, scope, parent, opts, 3, sub_chunk, sub_scope, pre_syms)
  end
  doc_special("let", {"[name1 val1 ... nameN valN]", "..."}, "Introduces a new scope in which a given set of local bindings are used.", true)
  local function get_prev_line(parent)
    if ("table" == type(parent)) then
      return get_prev_line((parent.leaf or parent[#parent]))
    else
      return (parent or "")
    end
  end
  local function needs_separator_3f(root, prev_line)
    return (root:match("^%(") and prev_line and not prev_line:find(" end$"))
  end
  SPECIALS.tset = function(ast, scope, parent)
    compiler.assert((3 < #ast), "expected table, key, and value arguments", ast)
    compiler.assert(((type(ast[2]) ~= "boolean") and (type(ast[2]) ~= "number")), "cannot set field of literal value", ast)
    local root = str1(compiler.compile1(ast[2], scope, parent, {nval = 1}))
    local root0 = nil
    if root:match("^[.{\"]") then
      root0 = string.format("(%s)", root)
    else
      root0 = root
    end
    local keys = nil
    do
      local tbl_17_ = {}
      local i_18_ = #tbl_17_
      for i = 3, (#ast - 1) do
        local val_19_ = str1(compiler.compile1(ast[i], scope, parent, {nval = 1}))
        if (nil ~= val_19_) then
          i_18_ = (i_18_ + 1)
          tbl_17_[i_18_] = val_19_
        end
      end
      keys = tbl_17_
    end
    local value = str1(compiler.compile1(ast[#ast], scope, parent, {nval = 1}))
    local fmtstr = nil
    if needs_separator_3f(root0, get_prev_line(parent)) then
      fmtstr = "do end %s[%s] = %s"
    else
      fmtstr = "%s[%s] = %s"
    end
    return compiler.emit(parent, fmtstr:format(root0, table.concat(keys, "]["), value), ast)
  end
  doc_special("tset", {"tbl", "key1", "...", "keyN", "val"}, "Set the value of a table field. Deprecated in favor of set.")
  local function calculate_if_target(scope, opts)
    if not (opts.tail or opts.target or opts.nval) then
      return "iife", true, nil
    elseif (opts.nval and (opts.nval ~= 0) and not opts.target) then
      local accum = {}
      local target_exprs = {}
      for i = 1, opts.nval do
        local s = compiler.gensym(scope)
        accum[i] = s
        target_exprs[i] = utils.expr(s, "sym")
      end
      return "target", opts.tail, table.concat(accum, ", "), target_exprs
    else
      return "none", opts.tail, opts.target
    end
  end
  local function if_2a(ast, scope, parent, opts)
    compiler.assert((2 < #ast), "expected condition and body", ast)
    if ((1 == (#ast % 2)) and (ast[(#ast - 1)] == true)) then
      table.remove(ast, (#ast - 1))
    end
    if (1 == (#ast % 2)) then
      table.insert(ast, utils.sym("nil"))
    end
    if (#ast == 2) then
      return SPECIALS["do"](utils.list(utils.sym("do"), ast[2]), scope, parent, opts)
    else
      local do_scope = compiler["make-scope"](scope)
      local branches = {}
      local wrapper, inner_tail, inner_target, target_exprs = calculate_if_target(scope, opts)
      local body_opts = {nval = opts.nval, tail = inner_tail, target = inner_target}
      local function compile_body(i)
        local chunk = {}
        local cscope = compiler["make-scope"](do_scope)
        compiler["keep-side-effects"](compiler.compile1(ast[i], cscope, chunk, body_opts), chunk, nil, ast[i])
        return {chunk = chunk, scope = cscope}
      end
      for i = 2, (#ast - 1), 2 do
        local condchunk = {}
        local _522_ = compiler.compile1(ast[i], do_scope, condchunk, {nval = 1})
        local cond = _522_[1]
        local branch = compile_body((i + 1))
        branch.cond = cond
        branch.condchunk = condchunk
        branch.nested = ((i ~= 2) and (next(condchunk, nil) == nil))
        table.insert(branches, branch)
      end
      local else_branch = compile_body(#ast)
      local s = compiler.gensym(scope)
      local buffer = {}
      local last_buffer = buffer
      for i = 1, #branches do
        local branch = branches[i]
        local fstr = nil
        if not branch.nested then
          fstr = "if %s then"
        else
          fstr = "elseif %s then"
        end
        local cond = tostring(branch.cond)
        local cond_line = fstr:format(cond)
        if branch.nested then
          compiler.emit(last_buffer, branch.condchunk, ast)
        else
          for _, v in ipairs(branch.condchunk) do
            compiler.emit(last_buffer, v, ast)
          end
        end
        compiler.emit(last_buffer, cond_line, ast)
        compiler.emit(last_buffer, branch.chunk, ast)
        if (i == #branches) then
          compiler.emit(last_buffer, "else", ast)
          compiler.emit(last_buffer, else_branch.chunk, ast)
          compiler.emit(last_buffer, "end", ast)
        elseif not branches[(i + 1)].nested then
          local next_buffer = {}
          compiler.emit(last_buffer, "else", ast)
          compiler.emit(last_buffer, next_buffer, ast)
          compiler.emit(last_buffer, "end", ast)
          last_buffer = next_buffer
        end
      end
      if (wrapper == "iife") then
        local iifeargs = ((scope.vararg and "...") or "")
        compiler.emit(parent, ("local function %s(%s)"):format(tostring(s), iifeargs), ast)
        compiler.emit(parent, buffer, ast)
        compiler.emit(parent, "end", ast)
        return utils.expr(("%s(%s)"):format(tostring(s), iifeargs), "statement")
      elseif (wrapper == "none") then
        for i = 1, #buffer do
          compiler.emit(parent, buffer[i], ast)
        end
        return {returned = true}
      else
        compiler.emit(parent, ("local %s"):format(inner_target), ast)
        for i = 1, #buffer do
          compiler.emit(parent, buffer[i], ast)
        end
        return target_exprs
      end
    end
  end
  SPECIALS["if"] = if_2a
  doc_special("if", {"cond1", "body1", "...", "condN", "bodyN"}, "Conditional form.\nTakes any number of condition/body pairs and evaluates the first body where\nthe condition evaluates to truthy. Similar to cond in other lisps.")
  local function clause_3f(v)
    return (utils["string?"](v) or (utils["sym?"](v) and not utils["multi-sym?"](v) and tostring(v):match("^&(.+)")))
  end
  local function remove_until_condition(bindings, ast)
    local _until = nil
    for i = (#bindings - 1), 3, -1 do
      local _528_0 = clause_3f(bindings[i])
      if ((_528_0 == false) or (_528_0 == nil)) then
      elseif (nil ~= _528_0) then
        local clause = _528_0
        compiler.assert(((clause == "until") and not _until), ("unexpected iterator clause: " .. clause), ast)
        table.remove(bindings, i)
        _until = table.remove(bindings, i)
      end
    end
    return _until
  end
  local function compile_until(_3fcondition, scope, chunk)
    if _3fcondition then
      local _530_ = compiler.compile1(_3fcondition, scope, chunk, {nval = 1})
      local condition_lua = _530_[1]
      return compiler.emit(chunk, ("if %s then break end"):format(tostring(condition_lua)), utils.expr(_3fcondition, "expression"))
    end
  end
  local function iterator_bindings(ast)
    local bindings = utils.copy(ast)
    local _3funtil = remove_until_condition(bindings, ast)
    local iter = table.remove(bindings)
    local bindings0 = nil
    if (1 == #bindings) then
      bindings0 = (utils["list?"](bindings[1]) or bindings)
    else
      for _, b in ipairs(bindings) do
        if utils["list?"](b) then
          utils.warn("unexpected parens in iterator", b)
        end
      end
      bindings0 = bindings
    end
    return bindings0, iter, _3funtil
  end
  SPECIALS.each = function(ast, scope, parent)
    compiler.assert((3 <= #ast), "expected body expression", ast[1])
    compiler.assert(utils["table?"](ast[2]), "expected binding table", ast)
    local sub_scope = compiler["make-scope"](scope)
    local binding, iter, _3funtil_condition = iterator_bindings(ast[2])
    local destructures = {}
    local deferred_scope_changes = {manglings = {}, symmeta = {}}
    utils.hook("pre-each", ast, sub_scope, binding, iter, _3funtil_condition)
    local function destructure_binding(v)
      if utils["sym?"](v) then
        return compiler["declare-local"](v, sub_scope, ast, nil, deferred_scope_changes)
      else
        local raw = utils.sym(compiler.gensym(sub_scope))
        destructures[raw] = v
        return compiler["declare-local"](raw, sub_scope, ast)
      end
    end
    local bind_vars = nil
    do
      local tbl_17_ = {}
      local i_18_ = #tbl_17_
      for _, b in ipairs(binding) do
        local val_19_ = destructure_binding(b)
        if (nil ~= val_19_) then
          i_18_ = (i_18_ + 1)
          tbl_17_[i_18_] = val_19_
        end
      end
      bind_vars = tbl_17_
    end
    local vals = compiler.compile1(iter, scope, parent)
    local val_names = nil
    do
      local tbl_17_ = {}
      local i_18_ = #tbl_17_
      for _, v in ipairs(vals) do
        local val_19_ = tostring(v)
        if (nil ~= val_19_) then
          i_18_ = (i_18_ + 1)
          tbl_17_[i_18_] = val_19_
        end
      end
      val_names = tbl_17_
    end
    local chunk = {}
    compiler.assert(bind_vars[1], "expected binding and iterator", ast)
    compiler.emit(parent, ("for %s in %s do"):format(table.concat(bind_vars, ", "), table.concat(val_names, ", ")), ast)
    for raw, args in utils.stablepairs(destructures) do
      compiler.destructure(args, raw, ast, sub_scope, chunk, {declaration = true, nomulti = true, symtype = "each"})
    end
    compiler["apply-deferred-scope-changes"](sub_scope, deferred_scope_changes, ast)
    compile_until(_3funtil_condition, sub_scope, chunk)
    compile_do(ast, sub_scope, chunk, 3)
    compiler.emit(parent, chunk, ast)
    return compiler.emit(parent, "end", ast)
  end
  doc_special("each", {"[key value (iterator)]", "..."}, "Runs the body once for each set of values provided by the given iterator.\nMost commonly used with ipairs for sequential tables or pairs for  undefined\norder, but can be used with any iterator.", true)
  local function while_2a(ast, scope, parent)
    local len1 = #parent
    local condition = compiler.compile1(ast[2], scope, parent, {nval = 1})[1]
    local len2 = #parent
    local sub_chunk = {}
    if (len1 ~= len2) then
      for i = (len1 + 1), len2 do
        table.insert(sub_chunk, parent[i])
        parent[i] = nil
      end
      compiler.emit(parent, "while true do", ast)
      compiler.emit(sub_chunk, ("if not %s then break end"):format(condition[1]), ast)
    else
      compiler.emit(parent, ("while " .. tostring(condition) .. " do"), ast)
    end
    compile_do(ast, compiler["make-scope"](scope), sub_chunk, 3)
    compiler.emit(parent, sub_chunk, ast)
    return compiler.emit(parent, "end", ast)
  end
  SPECIALS["while"] = while_2a
  doc_special("while", {"condition", "..."}, "The classic while loop. Evaluates body until a condition is non-truthy.", true)
  local function for_2a(ast, scope, parent)
    compiler.assert(utils["table?"](ast[2]), "expected binding table", ast)
    local ranges = setmetatable(utils.copy(ast[2]), getmetatable(ast[2]))
    local until_condition = remove_until_condition(ranges, ast)
    local binding_sym = table.remove(ranges, 1)
    local sub_scope = compiler["make-scope"](scope)
    local range_args = {}
    local chunk = {}
    compiler.assert(utils["sym?"](binding_sym), ("unable to bind %s %s"):format(type(binding_sym), tostring(binding_sym)), ast[2])
    compiler.assert((3 <= #ast), "expected body expression", ast[1])
    compiler.assert((#ranges <= 3), "unexpected arguments", ranges)
    compiler.assert((1 < #ranges), "expected range to include start and stop", ranges)
    utils.hook("pre-for", ast, sub_scope, binding_sym)
    for i = 1, math.min(#ranges, 3) do
      range_args[i] = str1(compiler.compile1(ranges[i], scope, parent, {nval = 1}))
    end
    compiler.emit(parent, ("for %s = %s do"):format(compiler["declare-local"](binding_sym, sub_scope, ast), table.concat(range_args, ", ")), ast)
    compile_until(until_condition, sub_scope, chunk)
    compile_do(ast, sub_scope, chunk, 3)
    compiler.emit(parent, chunk, ast)
    return compiler.emit(parent, "end", ast)
  end
  SPECIALS["for"] = for_2a
  doc_special("for", {"[index start stop step?]", "..."}, "Numeric loop construct.\nEvaluates body once for each value between start and stop (inclusive).", true)
  local function method_special_type(ast)
    if (utils["string?"](ast[3]) and utils["valid-lua-identifier?"](ast[3])) then
      return "native"
    elseif utils["sym?"](ast[2]) then
      return "nonnative"
    else
      return "binding"
    end
  end
  local function native_method_call(ast, _scope, _parent, target, args)
    local _539_ = ast
    local _ = _539_[1]
    local _0 = _539_[2]
    local method_string = _539_[3]
    local call_string = nil
    if ((target.type == "literal") or (target.type == "varg") or ((target.type == "expression") and not (target[1]):match("[%)%]]$") and not (target[1]):match("%.[%a_][%w_]*$"))) then
      call_string = "(%s):%s(%s)"
    else
      call_string = "%s:%s(%s)"
    end
    return utils.expr(string.format(call_string, tostring(target), method_string, table.concat(args, ", ")), "statement")
  end
  local function nonnative_method_call(ast, scope, parent, target, args)
    local method_string = str1(compiler.compile1(ast[3], scope, parent, {nval = 1}))
    local args0 = {tostring(target), unpack(args)}
    return utils.expr(string.format("%s[%s](%s)", tostring(target), method_string, table.concat(args0, ", ")), "statement")
  end
  local function binding_method_call(ast, scope, parent, target, args)
    local method_string = str1(compiler.compile1(ast[3], scope, parent, {nval = 1}))
    local target_local = compiler.gensym(scope, "tgt")
    local args0 = {target_local, unpack(args)}
    compiler.emit(parent, string.format("local %s = %s", target_local, tostring(target)))
    return utils.expr(string.format("(%s)[%s](%s)", target_local, method_string, table.concat(args0, ", ")), "statement")
  end
  local function method_call(ast, scope, parent)
    compiler.assert((2 < #ast), "expected at least 2 arguments", ast)
    local _541_ = compiler.compile1(ast[2], scope, parent, {nval = 1})
    local target = _541_[1]
    local args = {}
    for i = 4, #ast do
      local subexprs = nil
      local _542_
      if (i ~= #ast) then
        _542_ = 1
      else
      _542_ = nil
      end
      subexprs = compiler.compile1(ast[i], scope, parent, {nval = _542_})
      local tbl_17_ = args
      local i_18_ = #tbl_17_
      for _, subexpr in ipairs(subexprs) do
        local val_19_ = tostring(subexpr)
        if (nil ~= val_19_) then
          i_18_ = (i_18_ + 1)
          tbl_17_[i_18_] = val_19_
        end
      end
    end
    local _545_0 = method_special_type(ast)
    if (_545_0 == "native") then
      return native_method_call(ast, scope, parent, target, args)
    elseif (_545_0 == "nonnative") then
      return nonnative_method_call(ast, scope, parent, target, args)
    elseif (_545_0 == "binding") then
      return binding_method_call(ast, scope, parent, target, args)
    end
  end
  SPECIALS[":"] = method_call
  doc_special(":", {"tbl", "method-name", "..."}, "Call the named method on tbl with the provided args.\nMethod name doesn't have to be known at compile-time; if it is, use\n(tbl:method-name ...) instead.")
  SPECIALS.comment = function(ast, _, parent)
    local c = nil
    local _547_
    do
      local tbl_17_ = {}
      local i_18_ = #tbl_17_
      for i, elt in ipairs(ast) do
        local val_19_ = nil
        if (i ~= 1) then
          val_19_ = view(elt, {["one-line?"] = true})
        else
        val_19_ = nil
        end
        if (nil ~= val_19_) then
          i_18_ = (i_18_ + 1)
          tbl_17_[i_18_] = val_19_
        end
      end
      _547_ = tbl_17_
    end
    c = table.concat(_547_, " "):gsub("%]%]", "]\\]")
    return compiler.emit(parent, ("--[[ " .. c .. " ]]"), ast)
  end
  doc_special("comment", {"..."}, "Comment which will be emitted in Lua output.", true)
  local function hashfn_max_used(f_scope, i, max)
    local max0 = nil
    if f_scope.symmeta[("$" .. i)].used then
      max0 = i
    else
      max0 = max
    end
    if (i < 9) then
      return hashfn_max_used(f_scope, (i + 1), max0)
    else
      return max0
    end
  end
  SPECIALS.hashfn = function(ast, scope, parent)
    compiler.assert((#ast == 2), "expected one argument", ast)
    local f_scope = nil
    do
      local _552_0 = compiler["make-scope"](scope)
      _552_0["vararg"] = false
      _552_0["hashfn"] = true
      f_scope = _552_0
    end
    local f_chunk = {}
    local name = compiler.gensym(scope)
    local symbol = utils.sym(name)
    local args = {}
    compiler["declare-local"](symbol, scope, ast)
    for i = 1, 9 do
      args[i] = compiler["declare-local"](utils.sym(("$" .. i)), f_scope, ast)
    end
    local function walker(idx, node, _3fparent_node)
      if utils["sym?"](node, "$...") then
        f_scope.vararg = true
        if _3fparent_node then
          _3fparent_node[idx] = utils.varg()
          return nil
        else
          return utils.varg()
        end
      else
        return ((utils["list?"](node) and (not _3fparent_node or not utils["sym?"](node[1], "hashfn"))) or utils["table?"](node))
      end
    end
    utils["walk-tree"](ast, walker)
    compiler.compile1(ast[2], f_scope, f_chunk, {tail = true})
    local max_used = hashfn_max_used(f_scope, 1, 0)
    if f_scope.vararg then
      compiler.assert((max_used == 0), "$ and $... in hashfn are mutually exclusive", ast)
    end
    local arg_str = nil
    if f_scope.vararg then
      arg_str = tostring(utils.varg())
    else
      arg_str = table.concat(args, ", ", 1, max_used)
    end
    compiler.emit(parent, string.format("local function %s(%s)", name, arg_str), ast)
    compiler.emit(parent, f_chunk, ast)
    compiler.emit(parent, "end", ast)
    return utils.expr(name, "sym")
  end
  doc_special("hashfn", {"..."}, "Function literal shorthand; args are either $... OR $1, $2, etc.")
  local function comparator_special_type(ast)
    if (3 == #ast) then
      return "native"
    elseif utils["every?"]({unpack(ast, 3, (#ast - 1))}, utils["idempotent-expr?"]) then
      return "idempotent"
    else
      return "binding"
    end
  end
  local function short_circuit_safe_3f(x, scope)
    if (("table" ~= type(x)) or utils["sym?"](x) or utils["varg?"](x)) then
      return true
    elseif utils["table?"](x) then
      local ok = true
      for k, v in pairs(x) do
        if not ok then break end
        ok = (short_circuit_safe_3f(v, scope) and short_circuit_safe_3f(k, scope))
      end
      return ok
    elseif utils["list?"](x) then
      if utils["sym?"](x[1]) then
        local _558_0 = str1(x)
        if ((_558_0 == "fn") or (_558_0 == "hashfn") or (_558_0 == "let") or (_558_0 == "local") or (_558_0 == "var") or (_558_0 == "set") or (_558_0 == "tset") or (_558_0 == "if") or (_558_0 == "each") or (_558_0 == "for") or (_558_0 == "while") or (_558_0 == "do") or (_558_0 == "lua") or (_558_0 == "global")) then
          return false
        elseif (((_558_0 == "<") or (_558_0 == ">") or (_558_0 == "<=") or (_558_0 == ">=") or (_558_0 == "=") or (_558_0 == "not=") or (_558_0 == "~=")) and (comparator_special_type(x) == "binding")) then
          return false
        else
          local function _559_()
            return (1 ~= x[2])
          end
          if ((_558_0 == "pick-values") and _559_()) then
            return false
          else
            local function _560_()
              local call = _558_0
              return scope.macros[call]
            end
            if ((nil ~= _558_0) and _560_()) then
              local call = _558_0
              return false
            else
              local function _561_()
                return (method_special_type(x) == "binding")
              end
              if ((_558_0 == ":") and _561_()) then
                return false
              else
                local _ = _558_0
                local ok = true
                for i = 2, #x do
                  if not ok then break end
                  ok = short_circuit_safe_3f(x[i], scope)
                end
                return ok
              end
            end
          end
        end
      else
        local ok = true
        for _, v in ipairs(x) do
          if not ok then break end
          ok = short_circuit_safe_3f(v, scope)
        end
        return ok
      end
    end
  end
  local function operator_special_result(ast, zero_arity, unary_prefix, padded_op, operands)
    local _565_0 = #operands
    if (_565_0 == 0) then
      if zero_arity then
        return utils.expr(zero_arity, "literal")
      else
        return compiler.assert(false, "Expected more than 0 arguments", ast)
      end
    elseif (_565_0 == 1) then
      if unary_prefix then
        return ("(" .. unary_prefix .. padded_op .. operands[1] .. ")")
      else
        return operands[1]
      end
    else
      local _ = _565_0
      return ("(" .. table.concat(operands, padded_op) .. ")")
    end
  end
  local function emit_short_circuit_if(ast, scope, parent, name, subast, accumulator, expr_string, setter)
    if (accumulator ~= expr_string) then
      compiler.emit(parent, string.format(setter, accumulator, expr_string), ast)
    end
    local function _570_()
      if (name == "and") then
        return accumulator
      else
        return ("not " .. accumulator)
      end
    end
    compiler.emit(parent, ("if %s then"):format(_570_()), subast)
    do
      local chunk = {}
      compiler.compile1(subast, scope, chunk, {nval = 1, target = accumulator})
      compiler.emit(parent, chunk)
    end
    return compiler.emit(parent, "end")
  end
  local function operator_special(name, zero_arity, unary_prefix, ast, scope, parent)
    compiler.assert(not ((#ast == 2) and utils["varg?"](ast[2])), "tried to use vararg with operator", ast)
    local padded_op = (" " .. name .. " ")
    local operands, accumulator = {}
    if utils["call-of?"](ast[#ast], "values") then
      utils.warn("multiple values in operators are deprecated", ast)
    end
    for subast in iter_args(ast) do
      if ((nil ~= next(operands)) and ((name == "or") or (name == "and")) and not short_circuit_safe_3f(subast, scope)) then
        local expr_string = table.concat(operands, padded_op)
        local setter = nil
        if accumulator then
          setter = "%s = %s"
        else
          setter = "local %s = %s"
        end
        if not accumulator then
          accumulator = compiler.gensym(scope, name)
        end
        emit_short_circuit_if(ast, scope, parent, name, subast, accumulator, expr_string, setter)
        operands = {accumulator}
      else
        table.insert(operands, str1(compiler.compile1(subast, scope, parent, {nval = 1})))
      end
    end
    return operator_special_result(ast, zero_arity, unary_prefix, padded_op, operands)
  end
  local function define_arithmetic_special(name, zero_arity, unary_prefix, _3flua_name)
    local _576_
    do
      local _575_0 = (_3flua_name or name)
      local function _577_(...)
        return operator_special(_575_0, zero_arity, unary_prefix, ...)
      end
      _576_ = _577_
    end
    SPECIALS[name] = _576_
    return doc_special(name, {"a", "b", "..."}, "Arithmetic operator; works the same as Lua but accepts more arguments.")
  end
  define_arithmetic_special("+", "0", "0")
  define_arithmetic_special("..", "''")
  define_arithmetic_special("^")
  define_arithmetic_special("-", nil, "")
  define_arithmetic_special("*", "1", "1")
  define_arithmetic_special("%")
  define_arithmetic_special("/", nil, "1")
  define_arithmetic_special("//", nil, "1")
  SPECIALS["or"] = function(ast, scope, parent)
    return operator_special("or", "false", nil, ast, scope, parent)
  end
  SPECIALS["and"] = function(ast, scope, parent)
    return operator_special("and", "true", nil, ast, scope, parent)
  end
  doc_special("and", {"a", "b", "..."}, "Boolean operator; works the same as Lua but accepts more arguments.")
  doc_special("or", {"a", "b", "..."}, "Boolean operator; works the same as Lua but accepts more arguments.")
  local function bitop_special(native_name, lib_name, zero_arity, unary_prefix, ast, scope, parent)
    if (#ast == 1) then
      return compiler.assert(zero_arity, "Expected more than 0 arguments.", ast)
    else
      local len = #ast
      local operands = {}
      local padded_native_name = (" " .. native_name .. " ")
      local prefixed_lib_name = ("bit." .. lib_name)
      for i = 2, len do
        local subexprs = nil
        local _578_
        if (i ~= len) then
          _578_ = 1
        else
        _578_ = nil
        end
        subexprs = compiler.compile1(ast[i], scope, parent, {nval = _578_})
        local tbl_17_ = operands
        local i_18_ = #tbl_17_
        for _, s in ipairs(subexprs) do
          local val_19_ = tostring(s)
          if (nil ~= val_19_) then
            i_18_ = (i_18_ + 1)
            tbl_17_[i_18_] = val_19_
          end
        end
      end
      if (#operands == 1) then
        if utils.root.options.useBitLib then
          return (prefixed_lib_name .. "(" .. unary_prefix .. ", " .. operands[1] .. ")")
        else
          return ("(" .. unary_prefix .. padded_native_name .. operands[1] .. ")")
        end
      else
        if utils.root.options.useBitLib then
          return (prefixed_lib_name .. "(" .. table.concat(operands, ", ") .. ")")
        else
          return ("(" .. table.concat(operands, padded_native_name) .. ")")
        end
      end
    end
  end
  local function define_bitop_special(name, zero_arity, unary_prefix, native)
    local function _585_(...)
      return bitop_special(native, name, zero_arity, unary_prefix, ...)
    end
    SPECIALS[name] = _585_
    return nil
  end
  define_bitop_special("lshift", nil, "1", "<<")
  define_bitop_special("rshift", nil, "1", ">>")
  define_bitop_special("band", "-1", "-1", "&")
  define_bitop_special("bor", "0", "0", "|")
  define_bitop_special("bxor", "0", "0", "~")
  doc_special("lshift", {"x", "n"}, "Bitwise logical left shift of x by n bits.\nOnly works in Lua 5.3+ or LuaJIT with the --use-bit-lib flag.")
  doc_special("rshift", {"x", "n"}, "Bitwise logical right shift of x by n bits.\nOnly works in Lua 5.3+ or LuaJIT with the --use-bit-lib flag.")
  doc_special("band", {"x1", "x2", "..."}, "Bitwise AND of any number of arguments.\nOnly works in Lua 5.3+ or LuaJIT with the --use-bit-lib flag.")
  doc_special("bor", {"x1", "x2", "..."}, "Bitwise OR of any number of arguments.\nOnly works in Lua 5.3+ or LuaJIT with the --use-bit-lib flag.")
  doc_special("bxor", {"x1", "x2", "..."}, "Bitwise XOR of any number of arguments.\nOnly works in Lua 5.3+ or LuaJIT with the --use-bit-lib flag.")
  SPECIALS.bnot = function(ast, scope, parent)
    compiler.assert((#ast == 2), "expected one argument", ast)
    local _586_ = compiler.compile1(ast[2], scope, parent, {nval = 1})
    local value = _586_[1]
    if utils.root.options.useBitLib then
      return ("bit.bnot(" .. tostring(value) .. ")")
    else
      return ("~(" .. tostring(value) .. ")")
    end
  end
  doc_special("bnot", {"x"}, "Bitwise negation; only works in Lua 5.3+ or LuaJIT with the --use-bit-lib flag.")
  doc_special("..", {"a", "b", "..."}, "String concatenation operator; works the same as Lua but accepts more arguments.")
  local function native_comparator(op, _588_0, scope, parent)
    local _589_ = _588_0
    local _ = _589_[1]
    local lhs_ast = _589_[2]
    local rhs_ast = _589_[3]
    local _590_ = compiler.compile1(lhs_ast, scope, parent, {nval = 1})
    local lhs = _590_[1]
    local _591_ = compiler.compile1(rhs_ast, scope, parent, {nval = 1})
    local rhs = _591_[1]
    return string.format("(%s %s %s)", tostring(lhs), op, tostring(rhs))
  end
  local function idempotent_comparator(op, chain_op, ast, scope, parent)
    local vals = nil
    do
      local tbl_17_ = {}
      local i_18_ = #tbl_17_
      for i = 2, #ast do
        local val_19_ = str1(compiler.compile1(ast[i], scope, parent, {nval = 1}))
        if (nil ~= val_19_) then
          i_18_ = (i_18_ + 1)
          tbl_17_[i_18_] = val_19_
        end
      end
      vals = tbl_17_
    end
    local comparisons = nil
    do
      local tbl_17_ = {}
      local i_18_ = #tbl_17_
      for i = 1, (#vals - 1) do
        local val_19_ = string.format("(%s %s %s)", vals[i], op, vals[(i + 1)])
        if (nil ~= val_19_) then
          i_18_ = (i_18_ + 1)
          tbl_17_[i_18_] = val_19_
        end
      end
      comparisons = tbl_17_
    end
    local chain = string.format(" %s ", (chain_op or "and"))
    return ("(" .. table.concat(comparisons, chain) .. ")")
  end
  local function binding_comparator(op, chain_op, ast, scope, parent)
    local binding_left = {}
    local binding_right = {}
    local vals = {}
    local chain = string.format(" %s ", (chain_op or "and"))
    for i = 2, #ast do
      local compiled = str1(compiler.compile1(ast[i], scope, parent, {nval = 1}))
      if (utils["idempotent-expr?"](ast[i]) or (i == 2) or (i == #ast)) then
        table.insert(vals, compiled)
      else
        local my_sym = compiler.gensym(scope)
        table.insert(binding_left, my_sym)
        table.insert(binding_right, compiled)
        table.insert(vals, my_sym)
      end
    end
    compiler.emit(parent, string.format("local %s = %s", table.concat(binding_left, ", "), table.concat(binding_right, ", "), ast))
    local _595_
    do
      local tbl_17_ = {}
      local i_18_ = #tbl_17_
      for i = 1, (#vals - 1) do
        local val_19_ = string.format("(%s %s %s)", vals[i], op, vals[(i + 1)])
        if (nil ~= val_19_) then
          i_18_ = (i_18_ + 1)
          tbl_17_[i_18_] = val_19_
        end
      end
      _595_ = tbl_17_
    end
    return ("(" .. table.concat(_595_, chain) .. ")")
  end
  local function define_comparator_special(name, _3flua_op, _3fchain_op)
    do
      local op = (_3flua_op or name)
      local function opfn(ast, scope, parent)
        compiler.assert((2 < #ast), "expected at least two arguments", ast)
        local _597_0 = comparator_special_type(ast)
        if (_597_0 == "native") then
          return native_comparator(op, ast, scope, parent)
        elseif (_597_0 == "idempotent") then
          return idempotent_comparator(op, _3fchain_op, ast, scope, parent)
        elseif (_597_0 == "binding") then
          return binding_comparator(op, _3fchain_op, ast, scope, parent)
        else
          local _ = _597_0
          return error("internal compiler error. please report this to the fennel devs.")
        end
      end
      SPECIALS[name] = opfn
    end
    return doc_special(name, {"a", "b", "..."}, "Comparison operator; works the same as Lua but accepts more arguments.")
  end
  define_comparator_special(">")
  define_comparator_special("<")
  define_comparator_special(">=")
  define_comparator_special("<=")
  define_comparator_special("=", "==")
  define_comparator_special("not=", "~=", "or")
  local function define_unary_special(op, _3frealop)
    local function opfn(ast, scope, parent)
      compiler.assert((#ast == 2), "expected one argument", ast)
      local tail = compiler.compile1(ast[2], scope, parent, {nval = 1})
      return ((_3frealop or op) .. str1(tail))
    end
    SPECIALS[op] = opfn
    return nil
  end
  define_unary_special("not", "not ")
  doc_special("not", {"x"}, "Logical operator; works the same as Lua.")
  define_unary_special("length", "#")
  doc_special("length", {"x"}, "Returns the length of a table or string.")
  SPECIALS["~="] = SPECIALS["not="]
  SPECIALS["#"] = SPECIALS.length
  SPECIALS.quote = function(ast, scope, parent)
    compiler.assert((#ast == 2), "expected one argument", ast)
    local runtime, this_scope = true, scope
    while this_scope do
      this_scope = this_scope.parent
      if (this_scope == compiler.scopes.compiler) then
        runtime = false
      end
    end
    return compiler["do-quote"](ast[2], scope, parent, runtime)
  end
  doc_special("quote", {"x"}, "Quasiquote the following form. Only works in macro/compiler scope.")
  local macro_loaded = {}
  local function safe_getmetatable(tbl)
    local mt = getmetatable(tbl)
    assert((mt ~= getmetatable("")), "Illegal metatable access!")
    return mt
  end
  local safe_require = nil
  local function safe_compiler_env()
    local _601_
    do
      local _600_0 = rawget(_G, "utf8")
      if (nil ~= _600_0) then
        _601_ = utils.copy(_600_0)
      else
        _601_ = _600_0
      end
    end
    return {_VERSION = _VERSION, assert = assert, bit = rawget(_G, "bit"), error = error, getmetatable = safe_getmetatable, ipairs = ipairs, math = utils.copy(math), next = next, pairs = utils.stablepairs, pcall = pcall, print = print, rawequal = rawequal, rawget = rawget, rawlen = rawget(_G, "rawlen"), rawset = rawset, require = safe_require, select = select, setmetatable = setmetatable, string = utils.copy(string), table = utils.copy(table), tonumber = tonumber, tostring = tostring, type = type, utf8 = _601_, xpcall = xpcall}
  end
  local function combined_mt_pairs(env)
    local combined = {}
    local _603_ = getmetatable(env)
    local __index = _603_["__index"]
    if ("table" == type(__index)) then
      for k, v in pairs(__index) do
        combined[k] = v
      end
    end
    for k, v in next, env, nil do
      combined[k] = v
    end
    return next, combined, nil
  end
  local function make_compiler_env(ast, scope, parent, _3fopts)
    local provided = nil
    do
      local _605_0 = (_3fopts or utils.root.options)
      if ((_G.type(_605_0) == "table") and (_605_0["compiler-env"] == "strict")) then
        provided = safe_compiler_env()
      elseif ((_G.type(_605_0) == "table") and (nil ~= _605_0.compilerEnv)) then
        local compilerEnv = _605_0.compilerEnv
        provided = compilerEnv
      elseif ((_G.type(_605_0) == "table") and (nil ~= _605_0["compiler-env"])) then
        local compiler_env = _605_0["compiler-env"]
        provided = compiler_env
      else
        local _ = _605_0
        provided = safe_compiler_env()
      end
    end
    local env = nil
    local function _607_()
      return compiler.scopes.macro
    end
    local function _608_(symbol)
      compiler.assert(compiler.scopes.macro, "must call from macro", ast)
      return compiler.scopes.macro.manglings[tostring(symbol)]
    end
    local function _609_(base)
      return utils.sym(compiler.gensym((compiler.scopes.macro or scope), base))
    end
    local function _610_(form)
      compiler.assert(compiler.scopes.macro, "must call from macro", ast)
      return compiler.macroexpand(form, compiler.scopes.macro)
    end
    env = {["assert-compile"] = compiler.assert, ["ast-source"] = utils["ast-source"], ["comment?"] = utils["comment?"], ["fennel-module-name"] = fennel_module_name, ["get-scope"] = _607_, ["in-scope?"] = _608_, ["list?"] = utils["list?"], ["macro-loaded"] = macro_loaded, ["multi-sym?"] = utils["multi-sym?"], ["sequence?"] = utils["sequence?"], ["sym?"] = utils["sym?"], ["table?"] = utils["table?"], ["varg?"] = utils["varg?"], _AST = ast, _CHUNK = parent, _IS_COMPILER = true, _SCOPE = scope, _SPECIALS = compiler.scopes.global.specials, _VARARG = utils.varg(), comment = utils.comment, gensym = _609_, list = utils.list, macroexpand = _610_, sequence = utils.sequence, sym = utils.sym, unpack = unpack, version = utils.version, view = view}
    env._G = env
    return setmetatable(env, {__index = provided, __newindex = provided, __pairs = combined_mt_pairs})
  end
  local function _611_(...)
    local tbl_17_ = {}
    local i_18_ = #tbl_17_
    for c in string.gmatch((package.config or ""), "([^\n]+)") do
      local val_19_ = c
      if (nil ~= val_19_) then
        i_18_ = (i_18_ + 1)
        tbl_17_[i_18_] = val_19_
      end
    end
    return tbl_17_
  end
  local _613_ = _611_(...)
  local dirsep = _613_[1]
  local pathsep = _613_[2]
  local pathmark = _613_[3]
  local pkg_config = {dirsep = (dirsep or "/"), pathmark = (pathmark or "?"), pathsep = (pathsep or ";")}
  local function escapepat(str)
    return string.gsub(str, "[^%w]", "%%%1")
  end
  local function search_module(modulename, _3fpathstring)
    local pathsepesc = escapepat(pkg_config.pathsep)
    local pattern = ("([^%s]*)%s"):format(pathsepesc, pathsepesc)
    local no_dot_module = modulename:gsub("%.", pkg_config.dirsep)
    local fullpath = ((_3fpathstring or utils["fennel-module"].path) .. pkg_config.pathsep)
    local function try_path(path)
      local filename = path:gsub(escapepat(pkg_config.pathmark), no_dot_module)
      local filename2 = path:gsub(escapepat(pkg_config.pathmark), modulename)
      local _614_0 = (io.open(filename) or io.open(filename2))
      if (nil ~= _614_0) then
        local file = _614_0
        file:close()
        return filename
      else
        local _ = _614_0
        return nil, ("no file '" .. filename .. "'")
      end
    end
    local function find_in_path(start, _3ftried_paths)
      local _616_0 = fullpath:match(pattern, start)
      if (nil ~= _616_0) then
        local path = _616_0
        local _617_0, _618_0 = try_path(path)
        if (nil ~= _617_0) then
          local filename = _617_0
          return filename
        elseif ((_617_0 == nil) and (nil ~= _618_0)) then
          local error = _618_0
          local function _620_()
            local _619_0 = (_3ftried_paths or {})
            table.insert(_619_0, error)
            return _619_0
          end
          return find_in_path((start + #path + 1), _620_())
        end
      else
        local _ = _616_0
        local function _622_()
          local tried_paths = table.concat((_3ftried_paths or {}), "\n\9")
          if (_VERSION < "Lua 5.4") then
            return ("\n\9" .. tried_paths)
          else
            return tried_paths
          end
        end
        return nil, _622_()
      end
    end
    return find_in_path(1)
  end
  local function make_searcher(_3foptions)
    local function _625_(module_name)
      local opts = utils.copy(utils.root.options)
      for k, v in pairs((_3foptions or {})) do
        opts[k] = v
      end
      opts["module-name"] = module_name
      local _626_0, _627_0 = search_module(module_name)
      if (nil ~= _626_0) then
        local filename = _626_0
        local function _628_(...)
          return utils["fennel-module"].dofile(filename, opts, ...)
        end
        return _628_, filename
      elseif ((_626_0 == nil) and (nil ~= _627_0)) then
        local error = _627_0
        return error
      end
    end
    return _625_
  end
  local function dofile_with_searcher(fennel_macro_searcher, filename, opts, ...)
    local searchers = (package.loaders or package.searchers or {})
    local _ = table.insert(searchers, 1, fennel_macro_searcher)
    local m = utils["fennel-module"].dofile(filename, opts, ...)
    table.remove(searchers, 1)
    return m
  end
  local function fennel_macro_searcher(module_name)
    local opts = nil
    do
      local _630_0 = utils.copy(utils.root.options)
      _630_0["module-name"] = module_name
      _630_0["env"] = "_COMPILER"
      _630_0["requireAsInclude"] = false
      _630_0["allowedGlobals"] = nil
      opts = _630_0
    end
    local _631_0 = search_module(module_name, utils["fennel-module"]["macro-path"])
    if (nil ~= _631_0) then
      local filename = _631_0
      local _632_
      if (opts["compiler-env"] == _G) then
        local function _633_(...)
          return dofile_with_searcher(fennel_macro_searcher, filename, opts, ...)
        end
        _632_ = _633_
      else
        local function _634_(...)
          return utils["fennel-module"].dofile(filename, opts, ...)
        end
        _632_ = _634_
      end
      return _632_, filename
    end
  end
  local function lua_macro_searcher(module_name)
    local _637_0 = search_module(module_name, package.path)
    if (nil ~= _637_0) then
      local filename = _637_0
      local code = nil
      do
        local f = io.open(filename)
        local function close_handlers_10_(ok_11_, ...)
          f:close()
          if ok_11_ then
            return ...
          else
            return error(..., 0)
          end
        end
        local function _639_()
          return assert(f:read("*a"))
        end
        code = close_handlers_10_(_G.xpcall(_639_, (package.loaded.fennel or debug).traceback))
      end
      local chunk = load_code(code, make_compiler_env(), filename)
      return chunk, filename
    end
  end
  local macro_searchers = {fennel_macro_searcher, lua_macro_searcher}
  local function search_macro_module(modname, n)
    local _641_0 = macro_searchers[n]
    if (nil ~= _641_0) then
      local f = _641_0
      local _642_0, _643_0 = f(modname)
      if ((nil ~= _642_0) and true) then
        local loader = _642_0
        local _3ffilename = _643_0
        return loader, _3ffilename
      else
        local _ = _642_0
        return search_macro_module(modname, (n + 1))
      end
    end
  end
  local function sandbox_fennel_module(modname)
    if ((modname == "fennel.macros") or (package and package.loaded and ("table" == type(package.loaded[modname])) and (package.loaded[modname].metadata == compiler.metadata))) then
      local function _646_(_, ...)
        return (compiler.metadata):setall(...)
      end
      return {metadata = {setall = _646_}, view = view}
    end
  end
  local function _648_(modname)
    local function _649_()
      local loader, filename = search_macro_module(modname, 1)
      compiler.assert(loader, (modname .. " module not found."))
      macro_loaded[modname] = loader(modname, filename)
      return macro_loaded[modname]
    end
    return (macro_loaded[modname] or sandbox_fennel_module(modname) or _649_())
  end
  safe_require = _648_
  local function add_macros(macros_2a, ast, scope)
    compiler.assert(utils["table?"](macros_2a), "expected macros to be table", ast)
    for k, v in pairs(macros_2a) do
      compiler.assert((type(v) == "function"), "expected each macro to be function", ast)
      compiler["check-binding-valid"](utils.sym(k), scope, ast, {["macro?"] = true})
      scope.macros[k] = v
    end
    return nil
  end
  local function resolve_module_name(_650_0, _scope, _parent, opts)
    local _651_ = _650_0
    local second = _651_[2]
    local filename = _651_["filename"]
    local filename0 = (filename or (utils["table?"](second) and second.filename))
    local module_name = utils.root.options["module-name"]
    local modexpr = compiler.compile(second, opts)
    local modname_chunk = load_code(modexpr)
    return modname_chunk(module_name, filename0)
  end
  SPECIALS["require-macros"] = function(ast, scope, parent, _3freal_ast)
    compiler.assert((#ast == 2), "Expected one module name argument", (_3freal_ast or ast))
    local modname = resolve_module_name(ast, scope, parent, {})
    compiler.assert(utils["string?"](modname), "module name must compile to string", (_3freal_ast or ast))
    if not macro_loaded[modname] then
      local loader, filename = search_macro_module(modname, 1)
      compiler.assert(loader, (modname .. " module not found."), ast)
      macro_loaded[modname] = compiler.assert(utils["table?"](loader(modname, filename)), "expected macros to be table", (_3freal_ast or ast))
    end
    if ("import-macros" == str1(ast)) then
      return macro_loaded[modname]
    else
      return add_macros(macro_loaded[modname], ast, scope)
    end
  end
  doc_special("require-macros", {"macro-module-name"}, "Load given module and use its contents as macro definitions in current scope.\nDeprecated.")
  local function emit_included_fennel(src, path, opts, sub_chunk)
    local subscope = compiler["make-scope"](utils.root.scope.parent)
    local forms = {}
    if utils.root.options.requireAsInclude then
      subscope.specials.require = compiler["require-include"]
    end
    for _, val in parser.parser(parser["string-stream"](src), path) do
      table.insert(forms, val)
    end
    for i = 1, #forms do
      local subopts = nil
      if (i == #forms) then
        subopts = {tail = true}
      else
        subopts = {nval = 0}
      end
      utils["propagate-options"](opts, subopts)
      compiler.compile1(forms[i], subscope, sub_chunk, subopts)
    end
    return nil
  end
  local function include_path(ast, opts, path, mod, fennel_3f)
    utils.root.scope.includes[mod] = "fnl/loading"
    local src = nil
    do
      local f = assert(io.open(path))
      local function close_handlers_10_(ok_11_, ...)
        f:close()
        if ok_11_ then
          return ...
        else
          return error(..., 0)
        end
      end
      local function _657_()
        return assert(f:read("*all")):gsub("[\13\n]*$", "")
      end
      src = close_handlers_10_(_G.xpcall(_657_, (package.loaded.fennel or debug).traceback))
    end
    local ret = utils.expr(("require(\"" .. mod .. "\")"), "statement")
    local target = ("package.preload[%q]"):format(mod)
    local preload_str = (target .. " = " .. target .. " or function(...)")
    local temp_chunk, sub_chunk = {}, {}
    compiler.emit(temp_chunk, preload_str, ast)
    compiler.emit(temp_chunk, sub_chunk)
    compiler.emit(temp_chunk, "end", ast)
    for _, v in ipairs(temp_chunk) do
      table.insert(utils.root.chunk, v)
    end
    if fennel_3f then
      emit_included_fennel(src, path, opts, sub_chunk)
    else
      compiler.emit(sub_chunk, src, ast)
    end
    utils.root.scope.includes[mod] = ret
    return ret
  end
  local function include_circular_fallback(mod, modexpr, fallback, ast)
    if (utils.root.scope.includes[mod] == "fnl/loading") then
      compiler.assert(fallback, "circular include detected", ast)
      return fallback(modexpr)
    end
  end
  SPECIALS.include = function(ast, scope, parent, opts)
    compiler.assert((#ast == 2), "expected one argument", ast)
    local modexpr = nil
    do
      local _660_0, _661_0 = pcall(resolve_module_name, ast, scope, parent, opts)
      if ((_660_0 == true) and (nil ~= _661_0)) then
        local modname = _661_0
        modexpr = utils.expr(string.format("%q", modname), "literal")
      else
        local _ = _660_0
        modexpr = compiler.compile1(ast[2], scope, parent, {nval = 1})[1]
      end
    end
    if ((modexpr.type ~= "literal") or ((modexpr[1]):byte() ~= 34)) then
      if opts.fallback then
        return opts.fallback(modexpr)
      else
        return compiler.assert(false, "module name must be string literal", ast)
      end
    else
      local mod = load_code(("return " .. modexpr[1]))()
      local oldmod = utils.root.options["module-name"]
      local _ = nil
      utils.root.options["module-name"] = mod
      _ = nil
      local res = nil
      local function _665_()
        local _664_0 = search_module(mod)
        if (nil ~= _664_0) then
          local fennel_path = _664_0
          return include_path(ast, opts, fennel_path, mod, true)
        else
          local _0 = _664_0
          local lua_path = search_module(mod, package.path)
          if lua_path then
            return include_path(ast, opts, lua_path, mod, false)
          elseif opts.fallback then
            return opts.fallback(modexpr)
          else
            return compiler.assert(false, ("module not found " .. mod), ast)
          end
        end
      end
      res = ((utils["member?"](mod, (utils.root.options.skipInclude or {})) and opts.fallback(modexpr, true)) or include_circular_fallback(mod, modexpr, opts.fallback, ast) or utils.root.scope.includes[mod] or _665_())
      utils.root.options["module-name"] = oldmod
      return res
    end
  end
  doc_special("include", {"module-name-literal"}, "Like require but load the target module during compilation and embed it in the\nLua output. The module must be a string literal and resolvable at compile time.")
  local function eval_compiler_2a(ast, scope, parent)
    local env = make_compiler_env(ast, scope, parent)
    local opts = utils.copy(utils.root.options)
    opts.scope = compiler["make-scope"](compiler.scopes.compiler)
    opts.allowedGlobals = current_global_names(env)
    return assert(load_code(compiler.compile(ast, opts), wrap_env(env)))(opts["module-name"], ast.filename)
  end
  SPECIALS.macros = function(ast, scope, parent)
    compiler.assert((#ast == 2), "Expected one table argument", ast)
    local macro_tbl = eval_compiler_2a(ast[2], scope, parent)
    compiler.assert(utils["table?"](macro_tbl), "Expected one table argument", ast)
    return add_macros(macro_tbl, ast, scope)
  end
  doc_special("macros", {"{:macro-name-1 (fn [...] ...) ... :macro-name-N macro-body-N}"}, "Define all functions in the given table as macros local to the current scope.")
  SPECIALS["tail!"] = function(ast, scope, parent, opts)
    compiler.assert((#ast == 2), "Expected one argument", ast)
    local call = utils["list?"](compiler.macroexpand(ast[2], scope))
    local callee = tostring((call and utils["sym?"](call[1])))
    compiler.assert((call and not scope.specials[callee]), "Expected a function call as argument", ast)
    compiler.assert(opts.tail, "Must be in tail position", ast)
    return compiler.compile1(call, scope, parent, opts)
  end
  doc_special("tail!", {"body"}, "Assert that the body being called is in tail position.")
  SPECIALS["pick-values"] = function(ast, scope, parent)
    local n = ast[2]
    local vals = utils.list(utils.sym("values"), unpack(ast, 3))
    compiler.assert((("number" == type(n)) and (0 <= n) and (n == math.floor(n))), ("Expected n to be an integer >= 0, got " .. tostring(n)))
    if (1 == n) then
      local _669_ = compiler.compile1(vals, scope, parent, {nval = 1})
      local _670_ = _669_[1]
      local expr = _670_[1]
      return {("(" .. expr .. ")")}
    elseif (0 == n) then
      for i = 3, #ast do
        compiler["keep-side-effects"](compiler.compile1(ast[i], scope, parent, {nval = 0}), parent, nil, ast[i])
      end
      return {}
    else
      local syms = nil
      do
        local tbl_17_ = utils.list()
        local i_18_ = #tbl_17_
        for _ = 1, n do
          local val_19_ = utils.sym(compiler.gensym(scope, "pv"))
          if (nil ~= val_19_) then
            i_18_ = (i_18_ + 1)
            tbl_17_[i_18_] = val_19_
          end
        end
        syms = tbl_17_
      end
      compiler.destructure(syms, vals, ast, scope, parent, {declaration = true, nomulti = true, noundef = true, symtype = "pv"})
      return syms
    end
  end
  doc_special("pick-values", {"n", "..."}, "Evaluate to exactly n values.\n\nFor example,\n  (pick-values 2 ...)\nexpands to\n  (let [(_0_ _1_) ...]\n    (values _0_ _1_))")
  SPECIALS["eval-compiler"] = function(ast, scope, parent)
    local old_first = ast[1]
    ast[1] = utils.sym("do")
    local val = eval_compiler_2a(ast, scope, parent)
    ast[1] = old_first
    return val
  end
  doc_special("eval-compiler", {"..."}, "Evaluate the body at compile-time. Use the macro system instead if possible.", true)
  SPECIALS.unquote = function(ast)
    return compiler.assert(false, "tried to use unquote outside quote", ast)
  end
  doc_special("unquote", {"..."}, "Evaluate the argument even if it's in a quoted form.")
  return {["current-global-names"] = current_global_names, ["get-function-metadata"] = get_function_metadata, ["load-code"] = load_code, ["macro-loaded"] = macro_loaded, ["macro-searchers"] = macro_searchers, ["make-compiler-env"] = make_compiler_env, ["make-searcher"] = make_searcher, ["search-module"] = search_module, ["wrap-env"] = wrap_env, doc = doc_2a}
end
package.preload["fennel.compiler"] = package.preload["fennel.compiler"] or function(...)
  local utils = require("fennel.utils")
  local parser = require("fennel.parser")
  local friend = require("fennel.friend")
  local unpack = (table.unpack or _G.unpack)
  local scopes = {compiler = nil, global = nil, macro = nil}
  local function make_scope(_3fparent)
    local parent = (_3fparent or scopes.global)
    local _265_
    if parent then
      _265_ = ((parent.depth or 0) + 1)
    else
      _265_ = 0
    end
    return {["gensym-base"] = setmetatable({}, {__index = (parent and parent["gensym-base"])}), autogensyms = setmetatable({}, {__index = (parent and parent.autogensyms)}), depth = _265_, gensyms = setmetatable({}, {__index = (parent and parent.gensyms)}), hashfn = (parent and parent.hashfn), includes = setmetatable({}, {__index = (parent and parent.includes)}), macros = setmetatable({}, {__index = (parent and parent.macros)}), manglings = setmetatable({}, {__index = (parent and parent.manglings)}), parent = parent, refedglobals = {}, specials = setmetatable({}, {__index = (parent and parent.specials)}), symmeta = setmetatable({}, {__index = (parent and parent.symmeta)}), unmanglings = setmetatable({}, {__index = (parent and parent.unmanglings)}), vararg = (parent and parent.vararg)}
  end
  local function assert_msg(ast, msg)
    local ast_tbl = nil
    if ("table" == type(ast)) then
      ast_tbl = ast
    else
      ast_tbl = {}
    end
    local m = getmetatable(ast)
    local filename = ((m and m.filename) or ast_tbl.filename or "unknown")
    local line = ((m and m.line) or ast_tbl.line or "?")
    local col = ((m and m.col) or ast_tbl.col or "?")
    local target = tostring((utils["sym?"](ast_tbl[1]) or ast_tbl[1] or "()"))
    return string.format("%s:%s:%s: Compile error in '%s': %s", filename, line, col, target, msg)
  end
  local function assert_compile(condition, msg, ast, _3ffallback_ast)
    if not condition then
      local _268_ = (utils.root.options or {})
      local error_pinpoint = _268_["error-pinpoint"]
      local source = _268_["source"]
      local unfriendly = _268_["unfriendly"]
      local ast0 = nil
      if next(utils["ast-source"](ast)) then
        ast0 = ast
      else
        ast0 = (_3ffallback_ast or {})
      end
      if (nil == utils.hook("assert-compile", condition, msg, ast0, utils.root.reset)) then
        utils.root.reset()
        if unfriendly then
          error(assert_msg(ast0, msg), 0)
        else
          friend["assert-compile"](condition, msg, ast0, source, {["error-pinpoint"] = error_pinpoint})
        end
      end
    end
    return condition
  end
  scopes.global = make_scope()
  scopes.global.vararg = true
  scopes.compiler = make_scope(scopes.global)
  scopes.macro = scopes.global
  local function serialize_string(str)
    local function _273_(_241)
      return ("\\" .. _241:byte())
    end
    return string.gsub(string.gsub(string.gsub(string.format("%q", str), "\\\n", "\\n"), "\\9", "\\t"), "[\128-\255]", _273_)
  end
  local function global_mangling(str)
    if utils["valid-lua-identifier?"](str) then
      return str
    else
      local function _274_(_241)
        return string.format("_%02x", _241:byte())
      end
      return ("__fnl_global__" .. str:gsub("[^%w]", _274_))
    end
  end
  local function global_unmangling(identifier)
    local _276_0 = string.match(identifier, "^__fnl_global__(.*)$")
    if (nil ~= _276_0) then
      local rest = _276_0
      local _277_0 = nil
      local function _278_(_241)
        return string.char(tonumber(_241:sub(2), 16))
      end
      _277_0 = string.gsub(rest, "_[%da-f][%da-f]", _278_)
      return _277_0
    else
      local _ = _276_0
      return identifier
    end
  end
  local allowed_globals = nil
  local function global_allowed_3f(name)
    return (not allowed_globals or utils["member?"](name, allowed_globals))
  end
  local function unique_mangling(original, mangling, scope, append)
    if scope.unmanglings[mangling] then
      return unique_mangling(original, (original .. append), scope, (append + 1))
    else
      return mangling
    end
  end
  local function apply_deferred_scope_changes(scope, deferred_scope_changes, ast)
    for raw, mangled in pairs(deferred_scope_changes.manglings) do
      assert_compile(not scope.refedglobals[mangled], ("use of global " .. raw .. " is aliased by a local"), ast)
      scope.manglings[raw] = mangled
    end
    for raw, symmeta in pairs(deferred_scope_changes.symmeta) do
      scope.symmeta[raw] = symmeta
    end
    return nil
  end
  local function combine_parts(parts, scope)
    local ret = (scope.manglings[parts[1]] or global_mangling(parts[1]))
    for i = 2, #parts do
      if utils["valid-lua-identifier?"](parts[i]) then
        if (parts["multi-sym-method-call"] and (i == #parts)) then
          ret = (ret .. ":" .. parts[i])
        else
          ret = (ret .. "." .. parts[i])
        end
      else
        ret = (ret .. "[" .. serialize_string(parts[i]) .. "]")
      end
    end
    return ret
  end
  local function root_scope(scope)
    return ((utils.root and utils.root.scope) or (scope.parent and root_scope(scope.parent)) or scope)
  end
  local function next_append(root_scope_2a)
    root_scope_2a["gensym-append"] = ((root_scope_2a["gensym-append"] or 0) + 1)
    return ("_" .. root_scope_2a["gensym-append"] .. "_")
  end
  local function gensym(scope, _3fbase, _3fsuffix)
    local root_scope_2a = root_scope(scope)
    local mangling = ((_3fbase or "") .. next_append(root_scope_2a) .. (_3fsuffix or ""))
    while scope.unmanglings[mangling] do
      mangling = ((_3fbase or "") .. next_append(root_scope_2a) .. (_3fsuffix or ""))
    end
    if (_3fbase and (0 < #_3fbase)) then
      scope["gensym-base"][mangling] = _3fbase
    end
    scope.gensyms[mangling] = true
    return mangling
  end
  local function combine_auto_gensym(parts, first)
    parts[1] = first
    local last = table.remove(parts)
    local last2 = table.remove(parts)
    local last_joiner = ((parts["multi-sym-method-call"] and ":") or ".")
    table.insert(parts, (last2 .. last_joiner .. last))
    return table.concat(parts, ".")
  end
  local function autogensym(base, scope)
    local _284_0 = utils["multi-sym?"](base)
    if (nil ~= _284_0) then
      local parts = _284_0
      return combine_auto_gensym(parts, autogensym(parts[1], scope))
    else
      local _ = _284_0
      local function _285_()
        local mangling = gensym(scope, base:sub(1, -2), "auto")
        scope.autogensyms[base] = mangling
        return mangling
      end
      return (scope.autogensyms[base] or _285_())
    end
  end
  local function check_binding_valid(symbol, scope, ast, _3fopts)
    local name = tostring(symbol)
    local macro_3f = nil
    do
      local _287_0 = _3fopts
      if (nil ~= _287_0) then
        _287_0 = _287_0["macro?"]
      end
      macro_3f = _287_0
    end
    assert_compile(("&" ~= name:match("[&.:]")), "invalid character: &", symbol)
    assert_compile(not name:find("^%."), "invalid character: .", symbol)
    assert_compile(not (scope.specials[name] or (not macro_3f and scope.macros[name])), ("local %s was overshadowed by a special form or macro"):format(name), ast)
    return assert_compile(not utils["quoted?"](symbol), string.format("macro tried to bind %s without gensym", name), symbol)
  end
  local function declare_local(symbol, scope, ast, _3fvar_3f, _3fdeferred_scope_changes)
    check_binding_valid(symbol, scope, ast)
    assert_compile(not utils["multi-sym?"](symbol), ("unexpected multi symbol " .. tostring(symbol)), ast)
    local str = tostring(symbol)
    local raw = nil
    if (utils["lua-keyword?"](str) or str:match("^%d")) then
      raw = ("_" .. str)
    else
      raw = str
    end
    local mangling = nil
    local function _290_(_241)
      return string.format("_%02x", _241:byte())
    end
    mangling = string.gsub(string.gsub(raw, "-", "_"), "[^%w_]", _290_)
    local unique = unique_mangling(mangling, mangling, scope, 0)
    scope.unmanglings[unique] = (scope["gensym-base"][str] or str)
    do
      local target = (_3fdeferred_scope_changes or scope)
      target.manglings[str] = unique
      target.symmeta[str] = {symbol = symbol, var = _3fvar_3f}
    end
    return unique
  end
  local function hashfn_arg_name(name, multi_sym_parts, scope)
    if not scope.hashfn then
      return nil
    elseif (name == "$") then
      return "$1"
    elseif multi_sym_parts then
      if (multi_sym_parts and (multi_sym_parts[1] == "$")) then
        multi_sym_parts[1] = "$1"
      end
      return table.concat(multi_sym_parts, ".")
    end
  end
  local function symbol_to_expression(symbol, scope, _3freference_3f)
    utils.hook("symbol-to-expression", symbol, scope, _3freference_3f)
    local name = symbol[1]
    local multi_sym_parts = utils["multi-sym?"](name)
    local name0 = (hashfn_arg_name(name, multi_sym_parts, scope) or name)
    local parts = (multi_sym_parts or {name0})
    local etype = (((1 < #parts) and "expression") or "sym")
    local local_3f = scope.manglings[parts[1]]
    if (local_3f and scope.symmeta[parts[1]]) then
      scope.symmeta[parts[1]]["used"] = true
      symbol.referent = scope.symmeta[parts[1]].symbol
    end
    assert_compile(not scope.macros[parts[1]], "tried to reference a macro without calling it", symbol)
    assert_compile((not scope.specials[parts[1]] or ("require" == parts[1])), "tried to reference a special form without calling it", symbol)
    assert_compile((not _3freference_3f or local_3f or ("_ENV" == parts[1]) or global_allowed_3f(parts[1])), ("unknown identifier: " .. tostring(parts[1])), symbol)
    if (allowed_globals and not local_3f and scope.parent) then
      scope.parent.refedglobals[parts[1]] = true
    end
    return utils.expr(combine_parts(parts, scope), etype)
  end
  local function emit(chunk, out, _3fast)
    if (type(out) == "table") then
      return table.insert(chunk, out)
    else
      return table.insert(chunk, {ast = _3fast, leaf = out})
    end
  end
  local function peephole(chunk)
    if chunk.leaf then
      return chunk
    elseif ((3 <= #chunk) and (chunk[(#chunk - 2)].leaf == "do") and not chunk[(#chunk - 1)].leaf and (chunk[#chunk].leaf == "end")) then
      local kid = peephole(chunk[(#chunk - 1)])
      local new_chunk = {ast = chunk.ast}
      for i = 1, (#chunk - 3) do
        table.insert(new_chunk, peephole(chunk[i]))
      end
      for i = 1, #kid do
        table.insert(new_chunk, kid[i])
      end
      return new_chunk
    else
      local tbl_17_ = {}
      local i_18_ = #tbl_17_
      for _, x in ipairs(chunk) do
        local val_19_ = peephole(x)
        if (nil ~= val_19_) then
          i_18_ = (i_18_ + 1)
          tbl_17_[i_18_] = val_19_
        end
      end
      return tbl_17_
    end
  end
  local function flatten_chunk_correlated(main_chunk, options)
    local function flatten(chunk, out, last_line, file)
      local last_line0 = last_line
      if chunk.leaf then
        out[last_line0] = ((out[last_line0] or "") .. " " .. chunk.leaf)
      else
        for _, subchunk in ipairs(chunk) do
          if (subchunk.leaf or next(subchunk)) then
            local source = utils["ast-source"](subchunk.ast)
            if (file == source.filename) then
              last_line0 = math.max(last_line0, (source.line or 0))
            end
            last_line0 = flatten(subchunk, out, last_line0, file)
          end
        end
      end
      return last_line0
    end
    local out = {}
    local last = flatten(main_chunk, out, 1, options.filename)
    for i = 1, last do
      if (out[i] == nil) then
        out[i] = ""
      end
    end
    return table.concat(out, "\n")
  end
  local function flatten_chunk(file_sourcemap, chunk, tab, depth)
    if chunk.leaf then
      local _302_ = utils["ast-source"](chunk.ast)
      local endline = _302_["endline"]
      local filename = _302_["filename"]
      local line = _302_["line"]
      if ("end" == chunk.leaf) then
        table.insert(file_sourcemap, {filename, (endline or line)})
      else
        table.insert(file_sourcemap, {filename, line})
      end
      return chunk.leaf
    else
      local tab0 = nil
      do
        local _304_0 = tab
        if (_304_0 == true) then
          tab0 = "  "
        elseif (_304_0 == false) then
          tab0 = ""
        elseif (nil ~= _304_0) then
          local tab1 = _304_0
          tab0 = tab1
        elseif (_304_0 == nil) then
          tab0 = ""
        else
        tab0 = nil
        end
      end
      local _306_
      do
        local tbl_17_ = {}
        local i_18_ = #tbl_17_
        for _, c in ipairs(chunk) do
          local val_19_ = nil
          if (c.leaf or next(c)) then
            local sub = flatten_chunk(file_sourcemap, c, tab0, (depth + 1))
            if (0 < depth) then
              val_19_ = (tab0 .. sub:gsub("\n", ("\n" .. tab0)))
            else
              val_19_ = sub
            end
          else
          val_19_ = nil
          end
          if (nil ~= val_19_) then
            i_18_ = (i_18_ + 1)
            tbl_17_[i_18_] = val_19_
          end
        end
        _306_ = tbl_17_
      end
      return table.concat(_306_, "\n")
    end
  end
  local sourcemap = {}
  local function make_short_src(source)
    local source0 = source:gsub("\n", " ")
    if (#source0 <= 49) then
      return ("[fennel \"" .. source0 .. "\"]")
    else
      return ("[fennel \"" .. source0:sub(1, 46) .. "...\"]")
    end
  end
  local function flatten(chunk, options)
    local chunk0 = peephole(chunk)
    if options.correlate then
      return flatten_chunk_correlated(chunk0, options), {}
    else
      local file_sourcemap = {}
      local src = flatten_chunk(file_sourcemap, chunk0, options.indent, 0)
      file_sourcemap.short_src = (options.filename or make_short_src((options.source or src)))
      if options.filename then
        file_sourcemap.key = ("@" .. options.filename)
      else
        file_sourcemap.key = src
      end
      sourcemap[file_sourcemap.key] = file_sourcemap
      return src, file_sourcemap
    end
  end
  local function make_metadata()
    local function _314_(self, tgt, _3fkey)
      if self[tgt] then
        if (nil ~= _3fkey) then
          return self[tgt][_3fkey]
        else
          return self[tgt]
        end
      end
    end
    local function _317_(self, tgt, key, value)
      self[tgt] = (self[tgt] or {})
      self[tgt][key] = value
      return tgt
    end
    local function _318_(self, tgt, ...)
      local kv_len = select("#", ...)
      local kvs = {...}
      if ((kv_len % 2) ~= 0) then
        error("metadata:setall() expected even number of k/v pairs")
      end
      self[tgt] = (self[tgt] or {})
      for i = 1, kv_len, 2 do
        self[tgt][kvs[i]] = kvs[(i + 1)]
      end
      return tgt
    end
    return setmetatable({}, {__index = {get = _314_, set = _317_, setall = _318_}, __mode = "k"})
  end
  local function exprs1(exprs)
    local _320_
    do
      local tbl_17_ = {}
      local i_18_ = #tbl_17_
      for _, e in ipairs(exprs) do
        local val_19_ = tostring(e)
        if (nil ~= val_19_) then
          i_18_ = (i_18_ + 1)
          tbl_17_[i_18_] = val_19_
        end
      end
      _320_ = tbl_17_
    end
    return table.concat(_320_, ", ")
  end
  local function keep_side_effects(exprs, chunk, _3fstart, ast)
    for j = (_3fstart or 1), #exprs do
      local subexp = exprs[j]
      if ((subexp.type == "expression") and (subexp[1] ~= "nil")) then
        emit(chunk, ("do local _ = %s end"):format(tostring(subexp)), ast)
      elseif (subexp.type == "statement") then
        local code = tostring(subexp)
        local disambiguated = nil
        if (code:byte() == 40) then
          disambiguated = ("do end " .. code)
        else
          disambiguated = code
        end
        emit(chunk, disambiguated, ast)
      end
    end
    return nil
  end
  local function handle_compile_opts(exprs, parent, opts, ast)
    if opts.nval then
      local n = opts.nval
      local len = #exprs
      if (n ~= len) then
        if (n < len) then
          keep_side_effects(exprs, parent, (n + 1), ast)
          for i = (n + 1), len do
            exprs[i] = nil
          end
        else
          for i = (#exprs + 1), n do
            exprs[i] = utils.expr("nil", "literal")
          end
        end
      end
    end
    if opts.tail then
      emit(parent, string.format("return %s", exprs1(exprs)), ast)
    end
    if opts.target then
      local result = exprs1(exprs)
      local function _328_()
        if (result == "") then
          return "nil"
        else
          return result
        end
      end
      emit(parent, string.format("%s = %s", opts.target, _328_()), ast)
    end
    if (opts.tail or opts.target) then
      return {returned = true}
    else
      exprs["returned"] = true
      return exprs
    end
  end
  local function find_macro(ast, scope)
    local macro_2a = nil
    do
      local _331_0 = utils["sym?"](ast[1])
      if (_331_0 ~= nil) then
        local _332_0 = tostring(_331_0)
        if (_332_0 ~= nil) then
          macro_2a = scope.macros[_332_0]
        else
          macro_2a = _332_0
        end
      else
        macro_2a = _331_0
      end
    end
    local multi_sym_parts = utils["multi-sym?"](ast[1])
    if (not macro_2a and multi_sym_parts) then
      local nested_macro = utils["get-in"](scope.macros, multi_sym_parts)
      assert_compile((not scope.macros[multi_sym_parts[1]] or (type(nested_macro) == "function")), "macro not found in imported macro module", ast)
      return nested_macro
    else
      return macro_2a
    end
  end
  local function propagate_trace_info(_336_0, _index, node)
    local _337_ = _336_0
    local byteend = _337_["byteend"]
    local bytestart = _337_["bytestart"]
    local filename = _337_["filename"]
    local line = _337_["line"]
    do
      local src = utils["ast-source"](node)
      if (("table" == type(node)) and (filename ~= src.filename)) then
        src.filename, src.line, src["from-macro?"] = filename, line, true
        src.bytestart, src.byteend = bytestart, byteend
      end
    end
    return ("table" == type(node))
  end
  local function quote_literal_nils(index, node, parent)
    if (parent and utils["list?"](parent)) then
      for i = 1, utils.maxn(parent) do
        local _339_0 = parent[i]
        if (_339_0 == nil) then
          parent[i] = utils.sym("nil")
        end
      end
    end
    return index, node, parent
  end
  local function built_in_3f(m)
    local found_3f = false
    for _, f in pairs(scopes.global.macros) do
      if found_3f then break end
      found_3f = (f == m)
    end
    return found_3f
  end
  local function macroexpand_2a(ast, scope, _3fonce)
    local _342_0 = nil
    if utils["list?"](ast) then
      _342_0 = find_macro(ast, scope)
    else
    _342_0 = nil
    end
    if (_342_0 == false) then
      return ast
    elseif (nil ~= _342_0) then
      local macro_2a = _342_0
      local old_scope = scopes.macro
      local _ = nil
      scopes.macro = scope
      _ = nil
      local ok, transformed = nil, nil
      local function _344_()
        return macro_2a(unpack(ast, 2))
      end
      local function _345_()
        if built_in_3f(macro_2a) then
          return tostring
        else
          return debug.traceback
        end
      end
      ok, transformed = xpcall(_344_, _345_())
      local function _346_(...)
        return propagate_trace_info(ast, quote_literal_nils(...))
      end
      utils["walk-tree"](transformed, _346_)
      scopes.macro = old_scope
      assert_compile(ok, transformed, ast)
      utils.hook("macroexpand", ast, transformed, scope)
      if (_3fonce or not transformed) then
        return transformed
      else
        return macroexpand_2a(transformed, scope)
      end
    else
      local _ = _342_0
      return ast
    end
  end
  local function compile_special(ast, scope, parent, opts, special)
    local exprs = (special(ast, scope, parent, opts) or utils.expr("nil", "literal"))
    local exprs0 = nil
    if ("table" ~= type(exprs)) then
      exprs0 = utils.expr(exprs, "expression")
    else
      exprs0 = exprs
    end
    local exprs2 = nil
    if utils["expr?"](exprs0) then
      exprs2 = {exprs0}
    else
      exprs2 = exprs0
    end
    if not exprs2.returned then
      return handle_compile_opts(exprs2, parent, opts, ast)
    elseif (opts.tail or opts.target) then
      return {returned = true}
    else
      return exprs2
    end
  end
  local function callable_3f(_352_0, ctype, callee)
    local _353_ = _352_0
    local call_ast = _353_[1]
    if ("literal" == ctype) then
      return ("\"" == string.sub(callee, 1, 1))
    else
      return (utils["sym?"](call_ast) or utils["list?"](call_ast))
    end
  end
  local function compile_function_call(ast, scope, parent, opts, compile1, len)
    local _355_ = compile1(ast[1], scope, parent, {nval = 1})[1]
    local callee = _355_[1]
    local ctype = _355_["type"]
    local fargs = {}
    assert_compile(callable_3f(ast, ctype, callee), ("cannot call literal value " .. tostring(ast[1])), ast)
    for i = 2, len do
      local subexprs = nil
      local _356_
      if (i ~= len) then
        _356_ = 1
      else
      _356_ = nil
      end
      subexprs = compile1(ast[i], scope, parent, {nval = _356_})
      table.insert(fargs, subexprs[1])
      if (i == len) then
        for j = 2, #subexprs do
          table.insert(fargs, subexprs[j])
        end
      else
        keep_side_effects(subexprs, parent, 2, ast[i])
      end
    end
    local pat = nil
    if ("literal" == ctype) then
      pat = "(%s)(%s)"
    else
      pat = "%s(%s)"
    end
    local call = string.format(pat, tostring(callee), exprs1(fargs))
    return handle_compile_opts({utils.expr(call, "statement")}, parent, opts, ast)
  end
  local function compile_call(ast, scope, parent, opts, compile1)
    utils.hook("call", ast, scope)
    local len = #ast
    local first = ast[1]
    local multi_sym_parts = utils["multi-sym?"](first)
    local special = (utils["sym?"](first) and scope.specials[tostring(first)])
    assert_compile((0 < len), "expected a function, macro, or special to call", ast)
    if special then
      return compile_special(ast, scope, parent, opts, special)
    elseif (multi_sym_parts and multi_sym_parts["multi-sym-method-call"]) then
      local table_with_method = table.concat({unpack(multi_sym_parts, 1, (#multi_sym_parts - 1))}, ".")
      local method_to_call = multi_sym_parts[#multi_sym_parts]
      local new_ast = utils.list(utils.sym(":", ast), utils.sym(table_with_method, ast), method_to_call, select(2, unpack(ast)))
      return compile1(new_ast, scope, parent, opts)
    else
      return compile_function_call(ast, scope, parent, opts, compile1, len)
    end
  end
  local function compile_varg(ast, scope, parent, opts)
    local _361_
    if scope.hashfn then
      _361_ = "use $... in hashfn"
    else
      _361_ = "unexpected vararg"
    end
    assert_compile(scope.vararg, _361_, ast)
    return handle_compile_opts({utils.expr("...", "varg")}, parent, opts, ast)
  end
  local function compile_sym(ast, scope, parent, opts)
    local multi_sym_parts = utils["multi-sym?"](ast)
    assert_compile(not (multi_sym_parts and multi_sym_parts["multi-sym-method-call"]), "multisym method calls may only be in call position", ast)
    local e = nil
    if (ast[1] == "nil") then
      e = utils.expr("nil", "literal")
    else
      e = symbol_to_expression(ast, scope, true)
    end
    return handle_compile_opts({e}, parent, opts, ast)
  end
  local function serialize_number(n)
    local _364_0 = string.gsub(tostring(n), ",", ".")
    return _364_0
  end
  local function compile_scalar(ast, _scope, parent, opts)
    local serialize = nil
    do
      local _365_0 = type(ast)
      if (_365_0 == "nil") then
        serialize = tostring
      elseif (_365_0 == "boolean") then
        serialize = tostring
      elseif (_365_0 == "string") then
        serialize = serialize_string
      elseif (_365_0 == "number") then
        serialize = serialize_number
      else
      serialize = nil
      end
    end
    return handle_compile_opts({utils.expr(serialize(ast), "literal")}, parent, opts)
  end
  local function compile_table(ast, scope, parent, opts, compile1)
    local function escape_key(k)
      if ((type(k) == "string") and utils["valid-lua-identifier?"](k)) then
        return k
      else
        local _367_ = compile1(k, scope, parent, {nval = 1})
        local compiled = _367_[1]
        return ("[" .. tostring(compiled) .. "]")
      end
    end
    local keys = {}
    local buffer = nil
    do
      local tbl_17_ = {}
      local i_18_ = #tbl_17_
      for i, elem in ipairs(ast) do
        local val_19_ = nil
        do
          local nval = ((nil ~= ast[(i + 1)]) and 1)
          keys[i] = true
          val_19_ = exprs1(compile1(elem, scope, parent, {nval = nval}))
        end
        if (nil ~= val_19_) then
          i_18_ = (i_18_ + 1)
          tbl_17_[i_18_] = val_19_
        end
      end
      buffer = tbl_17_
    end
    do
      local tbl_17_ = buffer
      local i_18_ = #tbl_17_
      for k in utils.stablepairs(ast) do
        local val_19_ = nil
        if not keys[k] then
          local _370_ = compile1(ast[k], scope, parent, {nval = 1})
          local v = _370_[1]
          val_19_ = string.format("%s = %s", escape_key(k), tostring(v))
        else
        val_19_ = nil
        end
        if (nil ~= val_19_) then
          i_18_ = (i_18_ + 1)
          tbl_17_[i_18_] = val_19_
        end
      end
    end
    return handle_compile_opts({utils.expr(("{" .. table.concat(buffer, ", ") .. "}"), "expression")}, parent, opts, ast)
  end
  local function compile1(ast, scope, parent, _3fopts)
    local opts = (_3fopts or {})
    local ast0 = macroexpand_2a(ast, scope)
    if utils["list?"](ast0) then
      return compile_call(ast0, scope, parent, opts, compile1)
    elseif utils["varg?"](ast0) then
      return compile_varg(ast0, scope, parent, opts)
    elseif utils["sym?"](ast0) then
      return compile_sym(ast0, scope, parent, opts)
    elseif (type(ast0) == "table") then
      return compile_table(ast0, scope, parent, opts, compile1)
    elseif ((type(ast0) == "nil") or (type(ast0) == "boolean") or (type(ast0) == "number") or (type(ast0) == "string")) then
      return compile_scalar(ast0, scope, parent, opts)
    else
      return assert_compile(false, ("could not compile value of type " .. type(ast0)), ast0)
    end
  end
  local function destructure(to, from, ast, scope, parent, opts)
    local opts0 = (opts or {})
    local _374_ = opts0
    local declaration = _374_["declaration"]
    local forceglobal = _374_["forceglobal"]
    local forceset = _374_["forceset"]
    local isvar = _374_["isvar"]
    local symtype = _374_["symtype"]
    local symtype0 = ("_" .. (symtype or "dst"))
    local setter = nil
    if declaration then
      setter = "local %s = %s"
    else
      setter = "%s = %s"
    end
    local deferred_scope_changes = {manglings = {}, symmeta = {}}
    local function getname(symbol, ast0)
      local raw = symbol[1]
      assert_compile(not (opts0.nomulti and utils["multi-sym?"](raw)), ("unexpected multi symbol " .. raw), ast0)
      if declaration then
        return declare_local(symbol, scope, symbol, isvar, deferred_scope_changes)
      else
        local parts = (utils["multi-sym?"](raw) or {raw})
        local _376_ = parts
        local first = _376_[1]
        local meta = scope.symmeta[first]
        assert_compile(not raw:find(":"), "cannot set method sym", symbol)
        if ((#parts == 1) and not forceset) then
          assert_compile(not (forceglobal and meta), string.format("global %s conflicts with local", tostring(symbol)), symbol)
          assert_compile(not (meta and not meta.var), ("expected var " .. raw), symbol)
        end
        assert_compile((meta or not opts0.noundef or (scope.hashfn and ("$" == first)) or global_allowed_3f(first)), ("expected local " .. first), symbol)
        if forceglobal then
          assert_compile(not scope.symmeta[scope.unmanglings[raw]], ("global " .. raw .. " conflicts with local"), symbol)
          scope.manglings[raw] = global_mangling(raw)
          scope.unmanglings[global_mangling(raw)] = raw
          if allowed_globals then
            table.insert(allowed_globals, raw)
          end
        end
        return symbol_to_expression(symbol, scope)[1]
      end
    end
    local function compile_top_target(lvalues)
      local inits = nil
      do
        local tbl_17_ = {}
        local i_18_ = #tbl_17_
        for _, l in ipairs(lvalues) do
          local val_19_ = nil
          if scope.manglings[l] then
            val_19_ = l
          else
            val_19_ = "nil"
          end
          if (nil ~= val_19_) then
            i_18_ = (i_18_ + 1)
            tbl_17_[i_18_] = val_19_
          end
        end
        inits = tbl_17_
      end
      local init = table.concat(inits, ", ")
      local lvalue = table.concat(lvalues, ", ")
      local plast = parent[#parent]
      local plen = #parent
      local ret = compile1(from, scope, parent, {target = lvalue})
      if declaration then
        for pi = plen, #parent do
          if (parent[pi] == plast) then
            plen = pi
          end
        end
        if ((#parent == (plen + 1)) and parent[#parent].leaf) then
          parent[#parent]["leaf"] = ("local " .. parent[#parent].leaf)
        elseif (init == "nil") then
          table.insert(parent, (plen + 1), {ast = ast, leaf = ("local " .. lvalue)})
        else
          table.insert(parent, (plen + 1), {ast = ast, leaf = ("local " .. lvalue .. " = " .. init)})
        end
      end
      return ret
    end
    local function destructure_sym(left, rightexprs, up1, top_3f)
      local lname = getname(left, up1)
      check_binding_valid(left, scope, left)
      if top_3f then
        return compile_top_target({lname})
      else
        return emit(parent, setter:format(lname, exprs1(rightexprs)), left)
      end
    end
    local function dynamic_set_target(_387_0)
      local _388_ = _387_0
      local _ = _388_[1]
      local target = _388_[2]
      local keys = {(table.unpack or unpack)(_388_, 3)}
      assert_compile(utils["sym?"](target), "dynamic set needs symbol target", ast)
      assert_compile(scope.manglings[tostring(target)], ("unknown identifier: " .. tostring(target)), target)
      local keys0 = nil
      do
        local tbl_17_ = {}
        local i_18_ = #tbl_17_
        for _0, k in ipairs(keys) do
          local val_19_ = tostring(compile1(k, scope, parent, {nval = 1})[1])
          if (nil ~= val_19_) then
            i_18_ = (i_18_ + 1)
            tbl_17_[i_18_] = val_19_
          end
        end
        keys0 = tbl_17_
      end
      return string.format("%s[%s]", tostring(symbol_to_expression(target, scope, true)), table.concat(keys0, "]["))
    end
    local function destructure_values(left, rightexprs, up1, destructure1, top_3f)
      local left_names, tables = {}, {}
      for i, name in ipairs(left) do
        if utils["sym?"](name) then
          table.insert(left_names, getname(name, up1))
        elseif utils["call-of?"](name, ".") then
          table.insert(left_names, dynamic_set_target(name))
        else
          local symname = gensym(scope, symtype0)
          table.insert(left_names, symname)
          tables[i] = {name, utils.expr(symname, "sym")}
        end
      end
      assert_compile(left[1], "must provide at least one value", left)
      if top_3f then
        compile_top_target(left_names)
      elseif utils["expr?"](rightexprs) then
        emit(parent, setter:format(table.concat(left_names, ","), exprs1(rightexprs)), left)
      else
        local names = table.concat(left_names, ",")
        local target = nil
        if declaration then
          target = ("local " .. names)
        else
          target = names
        end
        emit(parent, compile1(rightexprs, scope, parent, {target = target}), left)
      end
      for _, pair in utils.stablepairs(tables) do
        destructure1(pair[1], {pair[2]}, left)
      end
      return nil
    end
    local unpack_fn = "function (t, k, e)\n                        local mt = getmetatable(t)\n                        if 'table' == type(mt) and mt.__fennelrest then\n                          return mt.__fennelrest(t, k)\n                        elseif e then\n                          local rest = {}\n                          for k, v in pairs(t) do\n                            if not e[k] then rest[k] = v end\n                          end\n                          return rest\n                        else\n                          return {(table.unpack or unpack)(t, k)}\n                        end\n                      end"
    local function destructure_kv_rest(s, v, left, excluded_keys, destructure1)
      local exclude_str = nil
      local _393_
      do
        local tbl_17_ = {}
        local i_18_ = #tbl_17_
        for _, k in ipairs(excluded_keys) do
          local val_19_ = string.format("[%s] = true", serialize_string(k))
          if (nil ~= val_19_) then
            i_18_ = (i_18_ + 1)
            tbl_17_[i_18_] = val_19_
          end
        end
        _393_ = tbl_17_
      end
      exclude_str = table.concat(_393_, ", ")
      local subexpr = utils.expr(string.format(string.gsub(("(" .. unpack_fn .. ")(%s, %s, {%s})"), "\n%s*", " "), s, tostring(v), exclude_str), "expression")
      return destructure1(v, {subexpr}, left)
    end
    local function destructure_rest(s, k, left, destructure1)
      local unpack_str = ("(" .. unpack_fn .. ")(%s, %s)")
      local formatted = string.format(string.gsub(unpack_str, "\n%s*", " "), s, k)
      local subexpr = utils.expr(formatted, "expression")
      local function _395_()
        local next_symbol = left[(k + 2)]
        return ((nil == next_symbol) or utils["sym?"](next_symbol, "&as"))
      end
      assert_compile((utils["sequence?"](left) and _395_()), "expected rest argument before last parameter", left)
      return destructure1(left[(k + 1)], {subexpr}, left)
    end
    local function optimize_table_destructure_3f(left, right)
      local function _396_()
        local all = next(left)
        for _, d in ipairs(left) do
          if not all then break end
          all = ((utils["sym?"](d) and not tostring(d):find("^&")) or (utils["list?"](d) and utils["sym?"](d[1], ".")))
        end
        return all
      end
      return (utils["sequence?"](left) and utils["sequence?"](right) and _396_())
    end
    local function destructure_table(left, rightexprs, top_3f, destructure1, up1)
      if optimize_table_destructure_3f(left, rightexprs) then
        return destructure_values(utils.list(unpack(left)), utils.list(utils.sym("values"), unpack(rightexprs)), up1, destructure1)
      else
        local right = nil
        do
          local _397_0 = nil
          if top_3f then
            _397_0 = exprs1(compile1(from, scope, parent))
          else
            _397_0 = exprs1(rightexprs)
          end
          if (_397_0 == "") then
            right = "nil"
          elseif (nil ~= _397_0) then
            local right0 = _397_0
            right = right0
          else
          right = nil
          end
        end
        local s = nil
        if utils["sym?"](rightexprs) then
          s = right
        else
          s = gensym(scope, symtype0)
        end
        local excluded_keys = {}
        if not utils["sym?"](rightexprs) then
          emit(parent, string.format("local %s = %s", s, right), left)
        end
        for k, v in utils.stablepairs(left) do
          if not (("number" == type(k)) and tostring(left[(k - 1)]):find("^&")) then
            if (utils["sym?"](k) and (tostring(k) == "&")) then
              destructure_kv_rest(s, v, left, excluded_keys, destructure1)
            elseif (utils["sym?"](v) and (tostring(v) == "&")) then
              destructure_rest(s, k, left, destructure1)
            elseif (utils["sym?"](k) and (tostring(k) == "&as")) then
              destructure_sym(v, {utils.expr(tostring(s))}, left)
            elseif (utils["sequence?"](left) and (tostring(v) == "&as")) then
              local _, next_sym, trailing = select(k, unpack(left))
              assert_compile((nil == trailing), "expected &as argument before last parameter", left)
              destructure_sym(next_sym, {utils.expr(tostring(s))}, left)
            else
              local key = nil
              if (type(k) == "string") then
                key = serialize_string(k)
              else
                key = k
              end
              local subexpr = utils.expr(("%s[%s]"):format(s, key), "expression")
              if (type(k) == "string") then
                table.insert(excluded_keys, k)
              end
              destructure1(v, subexpr, left)
            end
          end
        end
        return nil
      end
    end
    local function destructure1(left, rightexprs, up1, top_3f)
      if (utils["sym?"](left) and (left[1] ~= "nil")) then
        destructure_sym(left, rightexprs, up1, top_3f)
      elseif utils["table?"](left) then
        destructure_table(left, rightexprs, top_3f, destructure1, up1)
      elseif utils["call-of?"](left, ".") then
        destructure_values({left}, rightexprs, up1, destructure1)
      elseif utils["list?"](left) then
        assert_compile(top_3f, "can't nest multi-value destructuring", left)
        destructure_values(left, rightexprs, up1, destructure1, true)
      else
        assert_compile(false, string.format("unable to bind %s %s", type(left), tostring(left)), (((type(up1[2]) == "table") and up1[2]) or up1))
      end
      return (top_3f and {returned = true})
    end
    local ret = destructure1(to, from, ast, true)
    utils.hook("destructure", from, to, scope, opts0)
    apply_deferred_scope_changes(scope, deferred_scope_changes, ast)
    return ret
  end
  local function require_include(ast, scope, parent, opts)
    opts.fallback = function(e, no_warn)
      if not no_warn then
        utils.warn(("include module not found, falling back to require: %s"):format(tostring(e)), ast)
      end
      return utils.expr(string.format("require(%s)", tostring(e)), "statement")
    end
    return scopes.global.specials.include(ast, scope, parent, opts)
  end
  local function opts_for_compile(options)
    local opts = utils.copy(options)
    opts.indent = (opts.indent or "  ")
    allowed_globals = opts.allowedGlobals
    return opts
  end
  local function compile_asts(asts, options)
    local old_globals = allowed_globals
    local opts = opts_for_compile(options)
    local scope = (opts.scope or make_scope(scopes.global))
    local chunk = {}
    if opts.requireAsInclude then
      scope.specials.require = require_include
    end
    if opts.assertAsRepl then
      scope.macros.assert = scope.macros["assert-repl"]
    end
    local _411_ = utils.root
    _411_["set-reset"](_411_)
    utils.root.chunk, utils.root.scope, utils.root.options = chunk, scope, opts
    for i = 1, #asts do
      local exprs = compile1(asts[i], scope, chunk, {nval = (((i < #asts) and 0) or nil), tail = (i == #asts)})
      keep_side_effects(exprs, chunk, nil, asts[i])
      if (i == #asts) then
        utils.hook("chunk", asts[i], scope)
      end
    end
    allowed_globals = old_globals
    utils.root.reset()
    return flatten(chunk, opts)
  end
  local function compile_stream(stream, _3fopts)
    local opts = (_3fopts or {})
    local asts = nil
    do
      local tbl_17_ = {}
      local i_18_ = #tbl_17_
      for _, ast in parser.parser(stream, opts.filename, opts) do
        local val_19_ = ast
        if (nil ~= val_19_) then
          i_18_ = (i_18_ + 1)
          tbl_17_[i_18_] = val_19_
        end
      end
      asts = tbl_17_
    end
    return compile_asts(asts, opts)
  end
  local function compile_string(str, _3fopts)
    return compile_stream(parser["string-stream"](str, _3fopts), _3fopts)
  end
  local function compile(from, _3fopts)
    local _414_0 = type(from)
    if (_414_0 == "userdata") then
      local function _415_()
        local _416_0 = from:read(1)
        if (nil ~= _416_0) then
          return _416_0:byte()
        else
          return _416_0
        end
      end
      return compile_stream(_415_, _3fopts)
    elseif (_414_0 == "function") then
      return compile_stream(from, _3fopts)
    else
      local _ = _414_0
      return compile_asts({from}, _3fopts)
    end
  end
  local function traceback_frame(info)
    if ((info.what == "C") and info.name) then
      return string.format("\9[C]: in function '%s'", info.name)
    elseif (info.what == "C") then
      return "\9[C]: in ?"
    else
      local remap = sourcemap[info.source]
      if (remap and remap[info.currentline]) then
        if ((remap[info.currentline][1] or "unknown") ~= "unknown") then
          info.short_src = sourcemap[("@" .. remap[info.currentline][1])].short_src
        else
          info.short_src = remap.short_src
        end
        info.currentline = (remap[info.currentline][2] or -1)
      end
      if (info.what == "Lua") then
        local function _421_()
          if info.name then
            return ("'" .. info.name .. "'")
          else
            return "?"
          end
        end
        return string.format("\9%s:%d: in function %s", info.short_src, info.currentline, _421_())
      elseif (info.short_src == "(tail call)") then
        return "  (tail call)"
      else
        return string.format("\9%s:%d: in main chunk", info.short_src, info.currentline)
      end
    end
  end
  local lua_getinfo = debug.getinfo
  local function traceback(_3fmsg, _3fstart)
    local msg = tostring((_3fmsg or ""))
    if ((msg:find("^%g+:%d+:%d+ Compile error:.*") or msg:find("^%g+:%d+:%d+ Parse error:.*")) and not utils["debug-on?"]("trace")) then
      return msg
    else
      local lines = {}
      if (msg:find("^%g+:%d+:%d+ Compile error:") or msg:find("^%g+:%d+:%d+ Parse error:")) then
        table.insert(lines, msg)
      else
        local newmsg = msg:gsub("^[^:]*:%d+:%s+", "runtime error: ")
        table.insert(lines, newmsg)
      end
      table.insert(lines, "stack traceback:")
      local done_3f, level = false, (_3fstart or 2)
      while not done_3f do
        do
          local _425_0 = lua_getinfo(level, "Sln")
          if (_425_0 == nil) then
            done_3f = true
          elseif (nil ~= _425_0) then
            local info = _425_0
            table.insert(lines, traceback_frame(info))
          end
        end
        level = (level + 1)
      end
      return table.concat(lines, "\n")
    end
  end
  local function getinfo(thread_or_level, ...)
    local thread_or_level0 = nil
    if ("number" == type(thread_or_level)) then
      thread_or_level0 = (1 + thread_or_level)
    else
      thread_or_level0 = thread_or_level
    end
    local info = lua_getinfo(thread_or_level0, ...)
    local mapped = (info and sourcemap[info.source])
    if mapped then
      for _, key in ipairs({"currentline", "linedefined", "lastlinedefined"}) do
        local mapped_value = nil
        do
          local _429_0 = mapped
          if (nil ~= _429_0) then
            _429_0 = _429_0[info[key]]
          end
          if (nil ~= _429_0) then
            _429_0 = _429_0[2]
          end
          mapped_value = _429_0
        end
        if (info[key] and mapped_value) then
          info[key] = mapped_value
        end
      end
      if info.activelines then
        local tbl_14_ = {}
        for line in pairs(info.activelines) do
          local k_15_, v_16_ = mapped[line][2], true
          if ((k_15_ ~= nil) and (v_16_ ~= nil)) then
            tbl_14_[k_15_] = v_16_
          end
        end
        info.activelines = tbl_14_
      end
      if (info.what == "Lua") then
        info.what = "Fennel"
      end
    end
    return info
  end
  local function mixed_concat(t, joiner)
    local seen = {}
    local ret, s = "", ""
    for k, v in ipairs(t) do
      table.insert(seen, k)
      ret = (ret .. s .. v)
      s = joiner
    end
    for k, v in utils.stablepairs(t) do
      if not seen[k] then
        ret = (ret .. s .. "[" .. k .. "]" .. "=" .. v)
        s = joiner
      end
    end
    return ret
  end
  local function do_quote(form, scope, parent, runtime_3f)
    local function quote_all(form0, discard_non_numbers)
      local tbl_14_ = {}
      for k, v in utils.stablepairs(form0) do
        local k_15_, v_16_ = nil, nil
        if (type(k) == "number") then
          k_15_, v_16_ = k, do_quote(v, scope, parent, runtime_3f)
        elseif not discard_non_numbers then
          k_15_, v_16_ = do_quote(k, scope, parent, runtime_3f), do_quote(v, scope, parent, runtime_3f)
        else
        k_15_, v_16_ = nil
        end
        if ((k_15_ ~= nil) and (v_16_ ~= nil)) then
          tbl_14_[k_15_] = v_16_
        end
      end
      return tbl_14_
    end
    if utils["varg?"](form) then
      assert_compile(not runtime_3f, "quoted ... may only be used at compile time", form)
      return "_VARARG"
    elseif utils["sym?"](form) then
      local filename = nil
      if form.filename then
        filename = string.format("%q", form.filename)
      else
        filename = "nil"
      end
      local symstr = tostring(form)
      assert_compile(not runtime_3f, "symbols may only be used at compile time", form)
      if (symstr:find("#$") or symstr:find("#[:.]")) then
        return string.format("sym('%s', {filename=%s, line=%s})", autogensym(symstr, scope), filename, (form.line or "nil"))
      else
        return string.format("sym('%s', {quoted=true, filename=%s, line=%s})", symstr, filename, (form.line or "nil"))
      end
    elseif utils["call-of?"](form, "unquote") then
      local res = unpack(compile1(form[2], scope, parent))
      return res[1]
    elseif utils["list?"](form) then
      local mapped = quote_all(form, true)
      local filename = nil
      if form.filename then
        filename = string.format("%q", form.filename)
      else
        filename = "nil"
      end
      assert_compile(not runtime_3f, "lists may only be used at compile time", form)
      return string.format(("setmetatable({filename=%s, line=%s, bytestart=%s, %s}" .. ", getmetatable(list()))"), filename, (form.line or "nil"), (form.bytestart or "nil"), mixed_concat(mapped, ", "))
    elseif utils["sequence?"](form) then
      local mapped = quote_all(form)
      local source = getmetatable(form)
      local filename = nil
      if source.filename then
        filename = string.format("%q", source.filename)
      else
        filename = "nil"
      end
      local _444_
      if source then
        _444_ = source.line
      else
        _444_ = "nil"
      end
      return string.format("setmetatable({%s}, {filename=%s, line=%s, sequence=%s})", mixed_concat(mapped, ", "), filename, _444_, "(getmetatable(sequence()))['sequence']")
    elseif (type(form) == "table") then
      local mapped = quote_all(form)
      local source = getmetatable(form)
      local filename = nil
      if source.filename then
        filename = string.format("%q", source.filename)
      else
        filename = "nil"
      end
      local function _447_()
        if source then
          return source.line
        else
          return "nil"
        end
      end
      return string.format("setmetatable({%s}, {filename=%s, line=%s})", mixed_concat(mapped, ", "), filename, _447_())
    elseif (type(form) == "string") then
      return serialize_string(form)
    else
      return tostring(form)
    end
  end
  return {["apply-deferred-scope-changes"] = apply_deferred_scope_changes, ["check-binding-valid"] = check_binding_valid, ["compile-stream"] = compile_stream, ["compile-string"] = compile_string, ["declare-local"] = declare_local, ["do-quote"] = do_quote, ["global-allowed?"] = global_allowed_3f, ["global-mangling"] = global_mangling, ["global-unmangling"] = global_unmangling, ["keep-side-effects"] = keep_side_effects, ["make-scope"] = make_scope, ["require-include"] = require_include, ["symbol-to-expression"] = symbol_to_expression, assert = assert_compile, autogensym = autogensym, compile = compile, compile1 = compile1, destructure = destructure, emit = emit, gensym = gensym, getinfo = getinfo, macroexpand = macroexpand_2a, metadata = make_metadata(), scopes = scopes, sourcemap = sourcemap, traceback = traceback}
end
package.preload["fennel.friend"] = package.preload["fennel.friend"] or function(...)
  local utils = require("fennel.utils")
  local utf8_ok_3f, utf8 = pcall(require, "utf8")
  local suggestions = {["$ and $... in hashfn are mutually exclusive"] = {"modifying the hashfn so it only contains $... or $, $1, $2, $3, etc"}, ["can't introduce (.*) here"] = {"declaring the local at the top-level"}, ["can't start multisym segment with a digit"] = {"removing the digit", "adding a non-digit before the digit"}, ["cannot call literal value"] = {"checking for typos", "checking for a missing function name", "making sure to use prefix operators, not infix"}, ["could not compile value of type "] = {"debugging the macro you're calling to return a list or table"}, ["could not read number (.*)"] = {"removing the non-digit character", "beginning the identifier with a non-digit if it is not meant to be a number"}, ["expected a function.* to call"] = {"removing the empty parentheses", "using square brackets if you want an empty table"}, ["expected at least one pattern/body pair"] = {"adding a pattern and a body to execute when the pattern matches"}, ["expected binding and iterator"] = {"making sure you haven't omitted a local name or iterator"}, ["expected binding sequence"] = {"placing a table here in square brackets containing identifiers to bind"}, ["expected body expression"] = {"putting some code in the body of this form after the bindings"}, ["expected each macro to be function"] = {"ensuring that the value for each key in your macros table contains a function", "avoid defining nested macro tables"}, ["expected even number of name/value bindings"] = {"finding where the identifier or value is missing"}, ["expected even number of pattern/body pairs"] = {"checking that every pattern has a body to go with it", "adding _ before the final body"}, ["expected even number of values in table literal"] = {"removing a key", "adding a value"}, ["expected local"] = {"looking for a typo", "looking for a local which is used out of its scope"}, ["expected macros to be table"] = {"ensuring your macro definitions return a table"}, ["expected parameters"] = {"adding function parameters as a list of identifiers in brackets"}, ["expected range to include start and stop"] = {"adding missing arguments"}, ["expected rest argument before last parameter"] = {"moving & to right before the final identifier when destructuring"}, ["expected symbol for function parameter: (.*)"] = {"changing %s to an identifier instead of a literal value"}, ["expected var (.*)"] = {"declaring %s using var instead of let/local", "introducing a new local instead of changing the value of %s"}, ["expected vararg as last parameter"] = {"moving the \"...\" to the end of the parameter list"}, ["expected whitespace before opening delimiter"] = {"adding whitespace"}, ["global (.*) conflicts with local"] = {"renaming local %s"}, ["invalid character: (.)"] = {"deleting or replacing %s", "avoiding reserved characters like \", \\, ', ~, ;, @, `, and comma"}, ["local (.*) was overshadowed by a special form or macro"] = {"renaming local %s"}, ["macro not found in macro module"] = {"checking the keys of the imported macro module's returned table"}, ["macro tried to bind (.*) without gensym"] = {"changing to %s# when introducing identifiers inside macros"}, ["malformed multisym"] = {"ensuring each period or colon is not followed by another period or colon"}, ["may only be used at compile time"] = {"moving this to inside a macro if you need to manipulate symbols/lists", "using square brackets instead of parens to construct a table"}, ["method must be last component"] = {"using a period instead of a colon for field access", "removing segments after the colon", "making the method call, then looking up the field on the result"}, ["mismatched closing delimiter (.), expected (.)"] = {"replacing %s with %s", "deleting %s", "adding matching opening delimiter earlier"}, ["missing subject"] = {"adding an item to operate on"}, ["multisym method calls may only be in call position"] = {"using a period instead of a colon to reference a table's fields", "putting parens around this"}, ["tried to reference a macro without calling it"] = {"renaming the macro so as not to conflict with locals"}, ["tried to reference a special form without calling it"] = {"making sure to use prefix operators, not infix", "wrapping the special in a function if you need it to be first class"}, ["tried to use unquote outside quote"] = {"moving the form to inside a quoted form", "removing the comma"}, ["tried to use vararg with operator"] = {"accumulating over the operands"}, ["unable to bind (.*)"] = {"replacing the %s with an identifier"}, ["unexpected arguments"] = {"removing an argument", "checking for typos"}, ["unexpected closing delimiter (.)"] = {"deleting %s", "adding matching opening delimiter earlier"}, ["unexpected iterator clause"] = {"removing an argument", "checking for typos"}, ["unexpected multi symbol (.*)"] = {"removing periods or colons from %s"}, ["unexpected vararg"] = {"putting \"...\" at the end of the fn parameters if the vararg was intended"}, ["unknown identifier: (.*)"] = {"looking to see if there's a typo", "using the _G table instead, eg. _G.%s if you really want a global", "moving this code to somewhere that %s is in scope", "binding %s as a local in the scope of this code"}, ["unused local (.*)"] = {"renaming the local to _%s if it is meant to be unused", "fixing a typo so %s is used", "disabling the linter which checks for unused locals"}, ["use of global (.*) is aliased by a local"] = {"renaming local %s", "refer to the global using _G.%s instead of directly"}}
  local unpack = (table.unpack or _G.unpack)
  local function suggest(msg)
    local s = nil
    for pat, sug in pairs(suggestions) do
      if s then break end
      local matches = {msg:match(pat)}
      if next(matches) then
        local tbl_17_ = {}
        local i_18_ = #tbl_17_
        for _, s0 in ipairs(sug) do
          local val_19_ = s0:format(unpack(matches))
          if (nil ~= val_19_) then
            i_18_ = (i_18_ + 1)
            tbl_17_[i_18_] = val_19_
          end
        end
        s = tbl_17_
      else
      s = nil
      end
    end
    return s
  end
  local function read_line(filename, line, _3fsource)
    if _3fsource then
      local matcher = string.gmatch((_3fsource .. "\n"), "(.-)(\13?\n)")
      for _ = 2, line do
        matcher()
      end
      return matcher()
    else
      local f = assert(_G.io.open(filename))
      local function close_handlers_10_(ok_11_, ...)
        f:close()
        if ok_11_ then
          return ...
        else
          return error(..., 0)
        end
      end
      local function _181_()
        for _ = 2, line do
          f:read()
        end
        return f:read()
      end
      return close_handlers_10_(_G.xpcall(_181_, (package.loaded.fennel or debug).traceback))
    end
  end
  local function sub(str, start, _end)
    if ((_end < start) or (#str < start)) then
      return ""
    elseif utf8_ok_3f then
      return string.sub(str, utf8.offset(str, start), ((utf8.offset(str, (_end + 1)) or (utf8.len(str) + 1)) - 1))
    else
      return string.sub(str, start, math.min(_end, str:len()))
    end
  end
  local function highlight_line(codeline, col, _3fendcol, opts)
    if ((opts and (false == opts["error-pinpoint"])) or (os and os.getenv and os.getenv("NO_COLOR"))) then
      return codeline
    else
      local _184_ = (opts or {})
      local error_pinpoint = _184_["error-pinpoint"]
      local endcol = (_3fendcol or col)
      local eol = nil
      if utf8_ok_3f then
        eol = utf8.len(codeline)
      else
        eol = string.len(codeline)
      end
      local _186_ = (error_pinpoint or {"\27[7m", "\27[0m"})
      local open = _186_[1]
      local close = _186_[2]
      return (sub(codeline, 1, col) .. open .. sub(codeline, (col + 1), (endcol + 1)) .. close .. sub(codeline, (endcol + 2), eol))
    end
  end
  local function friendly_msg(msg, _188_0, source, opts)
    local _189_ = _188_0
    local col = _189_["col"]
    local endcol = _189_["endcol"]
    local endline = _189_["endline"]
    local filename = _189_["filename"]
    local line = _189_["line"]
    local ok, codeline = pcall(read_line, filename, line, source)
    local endcol0 = nil
    if (ok and codeline and (line ~= endline)) then
      endcol0 = #codeline
    else
      endcol0 = endcol
    end
    local out = {msg, ""}
    if (ok and codeline) then
      if col then
        table.insert(out, highlight_line(codeline, col, endcol0, opts))
      else
        table.insert(out, codeline)
      end
    end
    for _, suggestion in ipairs((suggest(msg) or {})) do
      table.insert(out, ("* Try %s."):format(suggestion))
    end
    return table.concat(out, "\n")
  end
  local function assert_compile(condition, msg, ast, source, opts)
    if not condition then
      local _193_ = utils["ast-source"](ast)
      local col = _193_["col"]
      local filename = _193_["filename"]
      local line = _193_["line"]
      error(friendly_msg(("%s:%s:%s: Compile error: %s"):format((filename or "unknown"), (line or "?"), (col or "?"), msg), utils["ast-source"](ast), source, opts), 0)
    end
    return condition
  end
  local function parse_error(msg, filename, line, col, source, opts)
    return error(friendly_msg(("%s:%s:%s: Parse error: %s"):format(filename, line, col, msg), {col = col, filename = filename, line = line}, source, opts), 0)
  end
  return {["assert-compile"] = assert_compile, ["parse-error"] = parse_error}
end
package.preload["fennel.parser"] = package.preload["fennel.parser"] or function(...)
  local utils = require("fennel.utils")
  local friend = require("fennel.friend")
  local unpack = (table.unpack or _G.unpack)
  local function granulate(getchunk)
    local c, index, done_3f = "", 1, false
    local function _195_(parser_state)
      if not done_3f then
        if (index <= #c) then
          local b = c:byte(index)
          index = (index + 1)
          return b
        else
          local _196_0 = getchunk(parser_state)
          local function _197_()
            local char = _196_0
            return (char ~= "")
          end
          if ((nil ~= _196_0) and _197_()) then
            local char = _196_0
            c = char
            index = 2
            return c:byte()
          else
            local _ = _196_0
            done_3f = true
            return nil
          end
        end
      end
    end
    local function _201_()
      c = ""
      return nil
    end
    return _195_, _201_
  end
  local function string_stream(str, _3foptions)
    local str0 = str:gsub("^#!", ";;")
    if _3foptions then
      _3foptions.source = str0
    end
    local index = 1
    local function _203_()
      local r = str0:byte(index)
      index = (index + 1)
      return r
    end
    return _203_
  end
  local delims = {[123] = 125, [125] = true, [40] = 41, [41] = true, [91] = 93, [93] = true}
  local function sym_char_3f(b)
    local b0 = nil
    if ("number" == type(b)) then
      b0 = b
    else
      b0 = string.byte(b)
    end
    return ((32 < b0) and not delims[b0] and (b0 ~= 127) and (b0 ~= 34) and (b0 ~= 39) and (b0 ~= 126) and (b0 ~= 59) and (b0 ~= 44) and (b0 ~= 64) and (b0 ~= 96))
  end
  local prefixes = {[35] = "hashfn", [39] = "quote", [44] = "unquote", [96] = "quote"}
  local function char_starter_3f(b)
    return (((1 < b) and (b < 127)) or ((192 < b) and (b < 247)))
  end
  local function parser_fn(getbyte, filename, _205_0)
    local _206_ = _205_0
    local options = _206_
    local comments = _206_["comments"]
    local source = _206_["source"]
    local unfriendly = _206_["unfriendly"]
    local stack = {}
    local line, byteindex, col, prev_col, lastb = 1, 0, 0, 0, nil
    local function ungetb(ub)
      if char_starter_3f(ub) then
        col = (col - 1)
      end
      if (ub == 10) then
        line, col = (line - 1), prev_col
      end
      byteindex = (byteindex - 1)
      lastb = ub
      return nil
    end
    local function getb()
      local r = nil
      if lastb then
        r, lastb = lastb, nil
      else
        r = getbyte({["stack-size"] = #stack})
      end
      if r then
        byteindex = (byteindex + 1)
      end
      if (r and char_starter_3f(r)) then
        col = (col + 1)
      end
      if (r == 10) then
        line, col, prev_col = (line + 1), 0, col
      end
      return r
    end
    local function whitespace_3f(b)
      local function _214_()
        local _213_0 = options.whitespace
        if (nil ~= _213_0) then
          _213_0 = _213_0[b]
        end
        return _213_0
      end
      return ((b == 32) or ((9 <= b) and (b <= 13)) or _214_())
    end
    local function parse_error(msg, _3fcol_adjust)
      local col0 = (col + (_3fcol_adjust or -1))
      if (nil == utils["hook-opts"]("parse-error", options, msg, filename, (line or "?"), col0, source, utils.root.reset)) then
        utils.root.reset()
        if unfriendly then
          return error(string.format("%s:%s:%s: Parse error: %s", filename, (line or "?"), col0, msg), 0)
        else
          return friend["parse-error"](msg, filename, (line or "?"), col0, source, options)
        end
      end
    end
    local function parse_stream()
      local whitespace_since_dispatch, done_3f, retval = true
      local function set_source_fields(source0)
        source0.byteend, source0.endcol, source0.endline = byteindex, (col - 1), line
        return nil
      end
      local function dispatch(v, _3fsource, _3fraw)
        whitespace_since_dispatch = false
        local v0 = nil
        do
          local _218_0 = utils["hook-opts"]("parse-form", options, v, _3fsource, _3fraw, stack)
          if (nil ~= _218_0) then
            local hookv = _218_0
            v0 = hookv
          else
            local _ = _218_0
            v0 = v
          end
        end
        local _220_0 = stack[#stack]
        if (_220_0 == nil) then
          retval, done_3f = v0, true
          return nil
        elseif ((_G.type(_220_0) == "table") and (nil ~= _220_0.prefix)) then
          local prefix = _220_0.prefix
          local source0 = nil
          do
            local _221_0 = table.remove(stack)
            set_source_fields(_221_0)
            source0 = _221_0
          end
          local list = utils.list(utils.sym(prefix, source0), v0)
          return dispatch(utils.copy(source0, list))
        elseif (nil ~= _220_0) then
          local top = _220_0
          return table.insert(top, v0)
        end
      end
      local function badend()
        local closers = nil
        do
          local tbl_17_ = {}
          local i_18_ = #tbl_17_
          for _, _223_0 in ipairs(stack) do
            local _224_ = _223_0
            local closer = _224_["closer"]
            local val_19_ = closer
            if (nil ~= val_19_) then
              i_18_ = (i_18_ + 1)
              tbl_17_[i_18_] = val_19_
            end
          end
          closers = tbl_17_
        end
        local _226_
        if (#stack == 1) then
          _226_ = ""
        else
          _226_ = "s"
        end
        return parse_error(string.format("expected closing delimiter%s %s", _226_, string.char(unpack(closers))))
      end
      local function skip_whitespace(b, close_table)
        if (b and whitespace_3f(b)) then
          whitespace_since_dispatch = true
          return skip_whitespace(getb(), close_table)
        elseif (not b and next(stack)) then
          badend()
          for i = #stack, 2, -1 do
            close_table(stack[i].closer)
          end
          return stack[1].closer
        else
          return b
        end
      end
      local function parse_comment(b, contents)
        if (b and (10 ~= b)) then
          local function _229_()
            table.insert(contents, string.char(b))
            return contents
          end
          return parse_comment(getb(), _229_())
        elseif comments then
          ungetb(10)
          return dispatch(utils.comment(table.concat(contents), {filename = filename, line = line}))
        end
      end
      local function open_table(b)
        if not whitespace_since_dispatch then
          parse_error(("expected whitespace before opening delimiter " .. string.char(b)))
        end
        return table.insert(stack, {bytestart = byteindex, closer = delims[b], col = (col - 1), filename = filename, line = line})
      end
      local function close_list(list)
        return dispatch(setmetatable(list, getmetatable(utils.list())))
      end
      local function close_sequence(tbl)
        local mt = getmetatable(utils.sequence())
        for k, v in pairs(tbl) do
          if ("number" ~= type(k)) then
            mt[k] = v
            tbl[k] = nil
          end
        end
        return dispatch(setmetatable(tbl, mt))
      end
      local function add_comment_at(comments0, index, node)
        local _233_0 = comments0[index]
        if (nil ~= _233_0) then
          local existing = _233_0
          return table.insert(existing, node)
        else
          local _ = _233_0
          comments0[index] = {node}
          return nil
        end
      end
      local function next_noncomment(tbl, i)
        if utils["comment?"](tbl[i]) then
          return next_noncomment(tbl, (i + 1))
        elseif utils["sym?"](tbl[i], ":") then
          return tostring(tbl[(i + 1)])
        else
          return tbl[i]
        end
      end
      local function extract_comments(tbl)
        local comments0 = {keys = {}, last = {}, values = {}}
        while utils["comment?"](tbl[#tbl]) do
          table.insert(comments0.last, 1, table.remove(tbl))
        end
        local last_key_3f = false
        for i, node in ipairs(tbl) do
          if not utils["comment?"](node) then
            last_key_3f = not last_key_3f
          elseif last_key_3f then
            add_comment_at(comments0.values, next_noncomment(tbl, i), node)
          else
            add_comment_at(comments0.keys, next_noncomment(tbl, i), node)
          end
        end
        for i = #tbl, 1, -1 do
          if utils["comment?"](tbl[i]) then
            table.remove(tbl, i)
          end
        end
        return comments0
      end
      local function close_curly_table(tbl)
        local comments0 = extract_comments(tbl)
        local keys = {}
        local val = {}
        if ((#tbl % 2) ~= 0) then
          byteindex = (byteindex - 1)
          parse_error("expected even number of values in table literal")
        end
        setmetatable(val, tbl)
        for i = 1, #tbl, 2 do
          if ((tostring(tbl[i]) == ":") and utils["sym?"](tbl[(i + 1)]) and utils["sym?"](tbl[i])) then
            tbl[i] = tostring(tbl[(i + 1)])
          end
          val[tbl[i]] = tbl[(i + 1)]
          table.insert(keys, tbl[i])
        end
        tbl.comments = comments0
        tbl.keys = keys
        return dispatch(val)
      end
      local function close_table(b)
        local top = table.remove(stack)
        if (top == nil) then
          parse_error(("unexpected closing delimiter " .. string.char(b)))
        end
        if (top.closer and (top.closer ~= b)) then
          parse_error(("mismatched closing delimiter " .. string.char(b) .. ", expected " .. string.char(top.closer)))
        end
        set_source_fields(top)
        if (b == 41) then
          return close_list(top)
        elseif (b == 93) then
          return close_sequence(top)
        else
          return close_curly_table(top)
        end
      end
      local function parse_string_loop(chars, b, state)
        if b then
          table.insert(chars, string.char(b))
        end
        local state0 = nil
        do
          local _244_0 = {state, b}
          if ((_G.type(_244_0) == "table") and (_244_0[1] == "base") and (_244_0[2] == 92)) then
            state0 = "backslash"
          elseif ((_G.type(_244_0) == "table") and (_244_0[1] == "base") and (_244_0[2] == 34)) then
            state0 = "done"
          elseif ((_G.type(_244_0) == "table") and (_244_0[1] == "backslash") and (_244_0[2] == 10)) then
            table.remove(chars, (#chars - 1))
            state0 = "base"
          else
            local _ = _244_0
            state0 = "base"
          end
        end
        if (b and (state0 ~= "done")) then
          return parse_string_loop(chars, getb(), state0)
        else
          return b
        end
      end
      local function escape_char(c)
        return ({nil, nil, nil, nil, nil, nil, "\\a", "\\b", "\\t", "\\n", "\\v", "\\f", "\\r"})[c:byte()]
      end
      local function parse_string(source0)
        if not whitespace_since_dispatch then
          utils.warn("expected whitespace before string", nil, filename, line)
        end
        table.insert(stack, {closer = 34})
        local chars = {"\""}
        if not parse_string_loop(chars, getb(), "base") then
          badend()
        end
        table.remove(stack)
        local raw = table.concat(chars)
        local formatted = raw:gsub("[\7-\13]", escape_char)
        local _249_0 = (rawget(_G, "loadstring") or load)(("return " .. formatted))
        if (nil ~= _249_0) then
          local load_fn = _249_0
          return dispatch(load_fn(), source0, raw)
        elseif (_249_0 == nil) then
          return parse_error(("Invalid string: " .. raw))
        end
      end
      local function parse_prefix(b)
        table.insert(stack, {bytestart = byteindex, col = (col - 1), filename = filename, line = line, prefix = prefixes[b]})
        local nextb = getb()
        local trailing_whitespace_3f = (whitespace_3f(nextb) or (true == delims[nextb]))
        if (trailing_whitespace_3f and (b ~= 35)) then
          parse_error("invalid whitespace after quoting prefix")
        end
        ungetb(nextb)
        if (trailing_whitespace_3f and (b == 35)) then
          local source0 = table.remove(stack)
          set_source_fields(source0)
          return dispatch(utils.sym("#", source0))
        end
      end
      local function parse_sym_loop(chars, b)
        if (b and sym_char_3f(b)) then
          table.insert(chars, string.char(b))
          return parse_sym_loop(chars, getb())
        else
          if b then
            ungetb(b)
          end
          return chars
        end
      end
      local function parse_number(rawstr, source0)
        local number_with_stripped_underscores = (not rawstr:find("^_") and rawstr:gsub("_", ""))
        if rawstr:match("^%d") then
          dispatch((tonumber(number_with_stripped_underscores) or parse_error(("could not read number \"" .. rawstr .. "\""))), source0, rawstr)
          return true
        else
          local _255_0 = tonumber(number_with_stripped_underscores)
          if (nil ~= _255_0) then
            local x = _255_0
            dispatch(x, source0, rawstr)
            return true
          else
            local _ = _255_0
            return false
          end
        end
      end
      local function check_malformed_sym(rawstr)
        local function col_adjust(pat)
          return (rawstr:find(pat) - utils.len(rawstr) - 1)
        end
        if (rawstr:match("^~") and (rawstr ~= "~=")) then
          parse_error("invalid character: ~")
        elseif (rawstr:match("[%.:][%.:]") and (rawstr ~= "..") and (rawstr ~= "$...")) then
          parse_error(("malformed multisym: " .. rawstr), col_adjust("[%.:][%.:]"))
        elseif ((rawstr ~= ":") and rawstr:match(":$")) then
          parse_error(("malformed multisym: " .. rawstr), col_adjust(":$"))
        elseif rawstr:match(":.+[%.:]") then
          parse_error(("method must be last component of multisym: " .. rawstr), col_adjust(":.+[%.:]"))
        end
        if not whitespace_since_dispatch then
          utils.warn("expected whitespace before token", nil, filename, line)
        end
        return rawstr
      end
      local function parse_sym(b)
        local source0 = {bytestart = byteindex, col = (col - 1), filename = filename, line = line}
        local rawstr = table.concat(parse_sym_loop({string.char(b)}, getb()))
        set_source_fields(source0)
        if (rawstr == "true") then
          return dispatch(true, source0)
        elseif (rawstr == "false") then
          return dispatch(false, source0)
        elseif (rawstr == "...") then
          return dispatch(utils.varg(source0))
        elseif rawstr:match("^:.+$") then
          return dispatch(rawstr:sub(2), source0, rawstr)
        elseif not parse_number(rawstr, source0) then
          return dispatch(utils.sym(check_malformed_sym(rawstr), source0))
        end
      end
      local function parse_loop(b)
        if not b then
        elseif (b == 59) then
          parse_comment(getb(), {";"})
        elseif (type(delims[b]) == "number") then
          open_table(b)
        elseif delims[b] then
          close_table(b)
        elseif (b == 34) then
          parse_string({bytestart = byteindex, col = col, filename = filename, line = line})
        elseif prefixes[b] then
          parse_prefix(b)
        elseif (sym_char_3f(b) or (b == string.byte("~"))) then
          parse_sym(b)
        elseif not utils["hook-opts"]("illegal-char", options, b, getb, ungetb, dispatch) then
          parse_error(("invalid character: " .. string.char(b)))
        end
        if not b then
          return nil
        elseif done_3f then
          return true, retval
        else
          return parse_loop(skip_whitespace(getb(), close_table))
        end
      end
      return parse_loop(skip_whitespace(getb(), close_table))
    end
    local function _263_()
      stack, line, byteindex, col, lastb = {}, 1, 0, 0, ((lastb ~= 10) and lastb)
      return nil
    end
    return parse_stream, _263_
  end
  local function parser(stream_or_string, _3ffilename, _3foptions)
    local filename = (_3ffilename or "unknown")
    local options = (_3foptions or utils.root.options or {})
    assert(("string" == type(filename)), "expected filename as second argument to parser")
    if ("string" == type(stream_or_string)) then
      return parser_fn(string_stream(stream_or_string, options), filename, options)
    else
      return parser_fn(stream_or_string, filename, options)
    end
  end
  return {["string-stream"] = string_stream, ["sym-char?"] = sym_char_3f, granulate = granulate, parser = parser}
end
local utils = nil
package.preload["fennel.view"] = package.preload["fennel.view"] or function(...)
  local type_order = {["function"] = 5, boolean = 2, number = 1, string = 3, table = 4, thread = 7, userdata = 6}
  local default_opts = {["detect-cycles?"] = true, ["empty-as-sequence?"] = false, ["escape-newlines?"] = false, ["line-length"] = 80, ["max-sparse-gap"] = 10, ["metamethod?"] = true, ["one-line?"] = false, ["prefer-colon?"] = false, ["utf8?"] = true, depth = 128}
  local lua_pairs = pairs
  local lua_ipairs = ipairs
  local function pairs(t)
    local _1_0 = getmetatable(t)
    if ((_G.type(_1_0) == "table") and (nil ~= _1_0.__pairs)) then
      local p = _1_0.__pairs
      return p(t)
    else
      local _ = _1_0
      return lua_pairs(t)
    end
  end
  local function ipairs(t)
    local _3_0 = getmetatable(t)
    if ((_G.type(_3_0) == "table") and (nil ~= _3_0.__ipairs)) then
      local i = _3_0.__ipairs
      return i(t)
    else
      local _ = _3_0
      return lua_ipairs(t)
    end
  end
  local function length_2a(t)
    local _5_0 = getmetatable(t)
    if ((_G.type(_5_0) == "table") and (nil ~= _5_0.__len)) then
      local l = _5_0.__len
      return l(t)
    else
      local _ = _5_0
      return #t
    end
  end
  local function get_default(key)
    local _7_0 = default_opts[key]
    if (_7_0 == nil) then
      return error(("option '%s' doesn't have a default value, use the :after key to set it"):format(tostring(key)))
    elseif (nil ~= _7_0) then
      local v = _7_0
      return v
    end
  end
  local function getopt(options, key)
    local _9_0 = options[key]
    if ((_G.type(_9_0) == "table") and (nil ~= _9_0.once)) then
      local val_2a = _9_0.once
      return val_2a
    else
      local _3fval = _9_0
      return _3fval
    end
  end
  local function normalize_opts(options)
    local tbl_14_ = {}
    for k, v in pairs(options) do
      local k_15_, v_16_ = nil, nil
      local function _12_()
        local _11_0 = v
        if ((_G.type(_11_0) == "table") and (nil ~= _11_0.after)) then
          local val = _11_0.after
          return val
        else
          local function _13_()
            return v.once
          end
          if ((_G.type(_11_0) == "table") and _13_()) then
            return get_default(k)
          else
            local _ = _11_0
            return v
          end
        end
      end
      k_15_, v_16_ = k, _12_()
      if ((k_15_ ~= nil) and (v_16_ ~= nil)) then
        tbl_14_[k_15_] = v_16_
      end
    end
    return tbl_14_
  end
  local function sort_keys(_16_0, _18_0)
    local _17_ = _16_0
    local a = _17_[1]
    local _19_ = _18_0
    local b = _19_[1]
    local ta = type(a)
    local tb = type(b)
    if ((ta == tb) and ((ta == "string") or (ta == "number"))) then
      return (a < b)
    else
      local dta = type_order[ta]
      local dtb = type_order[tb]
      if (dta and dtb) then
        return (dta < dtb)
      elseif dta then
        return true
      elseif dtb then
        return false
      else
        return (ta < tb)
      end
    end
  end
  local function max_index_gap(kv)
    local gap = 0
    if (0 < length_2a(kv)) then
      local i = 0
      for _, _22_0 in ipairs(kv) do
        local _23_ = _22_0
        local k = _23_[1]
        if (gap < (k - i)) then
          gap = (k - i)
        end
        i = k
      end
    end
    return gap
  end
  local function fill_gaps(kv)
    local missing_indexes = {}
    local i = 0
    for _, _26_0 in ipairs(kv) do
      local _27_ = _26_0
      local j = _27_[1]
      i = (i + 1)
      while (i < j) do
        table.insert(missing_indexes, i)
        i = (i + 1)
      end
    end
    for _, k in ipairs(missing_indexes) do
      table.insert(kv, k, {k})
    end
    return nil
  end
  local function table_kv_pairs(t, options)
    local assoc_3f = false
    local kv = {}
    local insert = table.insert
    for k, v in pairs(t) do
      if ((type(k) ~= "number") or (k < 1)) then
        assoc_3f = true
      end
      insert(kv, {k, v})
    end
    table.sort(kv, sort_keys)
    if not assoc_3f then
      if (options["max-sparse-gap"] < max_index_gap(kv)) then
        assoc_3f = true
      else
        fill_gaps(kv)
      end
    end
    if (length_2a(kv) == 0) then
      return kv, "empty"
    else
      local function _31_()
        if assoc_3f then
          return "table"
        else
          return "seq"
        end
      end
      return kv, _31_()
    end
  end
  local function count_table_appearances(t, appearances)
    if (type(t) == "table") then
      if not appearances[t] then
        appearances[t] = 1
        for k, v in pairs(t) do
          count_table_appearances(k, appearances)
          count_table_appearances(v, appearances)
        end
      else
        appearances[t] = ((appearances[t] or 0) + 1)
      end
    end
    return appearances
  end
  local function save_table(t, seen)
    local seen0 = (seen or {len = 0})
    local id = (seen0.len + 1)
    if not seen0[t] then
      seen0[t] = id
      seen0.len = id
    end
    return seen0
  end
  local function detect_cycle(t, seen)
    if ("table" == type(t)) then
      seen[t] = true
      local res = nil
      for k, v in pairs(t) do
        if res then break end
        res = (seen[k] or detect_cycle(k, seen) or seen[v] or detect_cycle(v, seen))
      end
      return res
    end
  end
  local function visible_cycle_3f(t, options)
    return (getopt(options, "detect-cycles?") and detect_cycle(t, {}) and save_table(t, options.seen) and (1 < (options.appearances[t] or 0)))
  end
  local function table_indent(indent, id)
    local opener_length = nil
    if id then
      opener_length = (length_2a(tostring(id)) + 2)
    else
      opener_length = 1
    end
    return (indent + opener_length)
  end
  local pp = nil
  local function concat_table_lines(elements, options, multiline_3f, indent, table_type, prefix, last_comment_3f)
    local indent_str = ("\n" .. string.rep(" ", indent))
    local open = nil
    local function _38_()
      if ("seq" == table_type) then
        return "["
      else
        return "{"
      end
    end
    open = ((prefix or "") .. _38_())
    local close = nil
    if ("seq" == table_type) then
      close = "]"
    else
      close = "}"
    end
    local oneline = (open .. table.concat(elements, " ") .. close)
    if (not getopt(options, "one-line?") and (multiline_3f or (options["line-length"] < (indent + length_2a(oneline))) or last_comment_3f)) then
      local function _40_()
        if last_comment_3f then
          return indent_str
        else
          return ""
        end
      end
      return (open .. table.concat(elements, indent_str) .. _40_() .. close)
    else
      return oneline
    end
  end
  local function utf8_len(x)
    local n = 0
    for _ in string.gmatch(x, "[%z\1-\127\192-\247]") do
      n = (n + 1)
    end
    return n
  end
  local function comment_3f(x)
    if ("table" == type(x)) then
      local fst = x[1]
      return (("string" == type(fst)) and (nil ~= fst:find("^;")))
    else
      return false
    end
  end
  local function pp_associative(t, kv, options, indent)
    local multiline_3f = false
    local id = options.seen[t]
    if (options.depth <= options.level) then
      return "{...}"
    elseif (id and getopt(options, "detect-cycles?")) then
      return ("@" .. id .. "{...}")
    else
      local visible_cycle_3f0 = visible_cycle_3f(t, options)
      local id0 = (visible_cycle_3f0 and options.seen[t])
      local indent0 = table_indent(indent, id0)
      local slength = nil
      if getopt(options, "utf8?") then
        slength = utf8_len
      else
        local function _43_(_241)
          return #_241
        end
        slength = _43_
      end
      local prefix = nil
      if visible_cycle_3f0 then
        prefix = ("@" .. id0)
      else
        prefix = ""
      end
      local items = nil
      do
        local options0 = normalize_opts(options)
        local tbl_17_ = {}
        local i_18_ = #tbl_17_
        for _, _46_0 in ipairs(kv) do
          local _47_ = _46_0
          local k = _47_[1]
          local v = _47_[2]
          local val_19_ = nil
          do
            local k0 = pp(k, options0, (indent0 + 1), true)
            local v0 = pp(v, options0, (indent0 + slength(k0) + 1))
            multiline_3f = (multiline_3f or k0:find("\n") or v0:find("\n"))
            val_19_ = (k0 .. " " .. v0)
          end
          if (nil ~= val_19_) then
            i_18_ = (i_18_ + 1)
            tbl_17_[i_18_] = val_19_
          end
        end
        items = tbl_17_
      end
      return concat_table_lines(items, options, multiline_3f, indent0, "table", prefix, false)
    end
  end
  local function pp_sequence(t, kv, options, indent)
    local multiline_3f = false
    local id = options.seen[t]
    if (options.depth <= options.level) then
      return "[...]"
    elseif (id and getopt(options, "detect-cycles?")) then
      return ("@" .. id .. "[...]")
    else
      local visible_cycle_3f0 = visible_cycle_3f(t, options)
      local id0 = (visible_cycle_3f0 and options.seen[t])
      local indent0 = table_indent(indent, id0)
      local prefix = nil
      if visible_cycle_3f0 then
        prefix = ("@" .. id0)
      else
        prefix = ""
      end
      local last_comment_3f = comment_3f(t[#t])
      local items = nil
      do
        local options0 = normalize_opts(options)
        local tbl_17_ = {}
        local i_18_ = #tbl_17_
        for _, _51_0 in ipairs(kv) do
          local _52_ = _51_0
          local _0 = _52_[1]
          local v = _52_[2]
          local val_19_ = nil
          do
            local v0 = pp(v, options0, indent0)
            multiline_3f = (multiline_3f or v0:find("\n") or v0:find("^;"))
            val_19_ = v0
          end
          if (nil ~= val_19_) then
            i_18_ = (i_18_ + 1)
            tbl_17_[i_18_] = val_19_
          end
        end
        items = tbl_17_
      end
      return concat_table_lines(items, options, multiline_3f, indent0, "seq", prefix, last_comment_3f)
    end
  end
  local function concat_lines(lines, options, indent, force_multi_line_3f)
    if (length_2a(lines) == 0) then
      if getopt(options, "empty-as-sequence?") then
        return "[]"
      else
        return "{}"
      end
    else
      local oneline = nil
      local _56_
      do
        local tbl_17_ = {}
        local i_18_ = #tbl_17_
        for _, line in ipairs(lines) do
          local val_19_ = line:gsub("^%s+", "")
          if (nil ~= val_19_) then
            i_18_ = (i_18_ + 1)
            tbl_17_[i_18_] = val_19_
          end
        end
        _56_ = tbl_17_
      end
      oneline = table.concat(_56_, " ")
      if (not getopt(options, "one-line?") and (force_multi_line_3f or oneline:find("\n") or (options["line-length"] < (indent + length_2a(oneline))))) then
        return table.concat(lines, ("\n" .. string.rep(" ", indent)))
      else
        return oneline
      end
    end
  end
  local function pp_metamethod(t, metamethod, options, indent)
    if (options.depth <= options.level) then
      if getopt(options, "empty-as-sequence?") then
        return "[...]"
      else
        return "{...}"
      end
    else
      local _ = nil
      local function _61_(_241)
        return visible_cycle_3f(_241, options)
      end
      options["visible-cycle?"] = _61_
      _ = nil
      local lines, force_multi_line_3f = nil, nil
      do
        local options0 = normalize_opts(options)
        lines, force_multi_line_3f = metamethod(t, pp, options0, indent)
      end
      options["visible-cycle?"] = nil
      local _62_0 = type(lines)
      if (_62_0 == "string") then
        return lines
      elseif (_62_0 == "table") then
        return concat_lines(lines, options, indent, force_multi_line_3f)
      else
        local _0 = _62_0
        return error("__fennelview metamethod must return a table of lines")
      end
    end
  end
  local function pp_table(x, options, indent)
    options.level = (options.level + 1)
    local x0 = nil
    do
      local _65_0 = nil
      if getopt(options, "metamethod?") then
        local _66_0 = x
        if (nil ~= _66_0) then
          local _67_0 = getmetatable(_66_0)
          if (nil ~= _67_0) then
            _65_0 = _67_0.__fennelview
          else
            _65_0 = _67_0
          end
        else
          _65_0 = _66_0
        end
      else
      _65_0 = nil
      end
      if (nil ~= _65_0) then
        local metamethod = _65_0
        x0 = pp_metamethod(x, metamethod, options, indent)
      else
        local _ = _65_0
        local _71_0, _72_0 = table_kv_pairs(x, options)
        if (true and (_72_0 == "empty")) then
          local _0 = _71_0
          if getopt(options, "empty-as-sequence?") then
            x0 = "[]"
          else
            x0 = "{}"
          end
        elseif ((nil ~= _71_0) and (_72_0 == "table")) then
          local kv = _71_0
          x0 = pp_associative(x, kv, options, indent)
        elseif ((nil ~= _71_0) and (_72_0 == "seq")) then
          local kv = _71_0
          x0 = pp_sequence(x, kv, options, indent)
        else
        x0 = nil
        end
      end
    end
    options.level = (options.level - 1)
    return x0
  end
  local function number__3estring(n)
    local _76_0 = string.gsub(tostring(n), ",", ".")
    return _76_0
  end
  local function colon_string_3f(s)
    return s:find("^[-%w?^_!$%&*+./|<=>]+$")
  end
  local utf8_inits = {{["max-byte"] = 127, ["max-code"] = 127, ["min-byte"] = 0, ["min-code"] = 0, len = 1}, {["max-byte"] = 223, ["max-code"] = 2047, ["min-byte"] = 192, ["min-code"] = 128, len = 2}, {["max-byte"] = 239, ["max-code"] = 65535, ["min-byte"] = 224, ["min-code"] = 2048, len = 3}, {["max-byte"] = 247, ["max-code"] = 1114111, ["min-byte"] = 240, ["min-code"] = 65536, len = 4}}
  local function default_byte_escape(byte, _options)
    return ("\\%03d"):format(byte)
  end
  local function utf8_escape(str, options)
    local function validate_utf8(str0, index)
      local inits = utf8_inits
      local byte = string.byte(str0, index)
      local init = nil
      do
        local ret = nil
        for _, init0 in ipairs(inits) do
          if ret then break end
          ret = (byte and (function(_77_,_78_,_79_) return (_77_ <= _78_) and (_78_ <= _79_) end)(init0["min-byte"],byte,init0["max-byte"]) and init0)
        end
        init = ret
      end
      local code = nil
      local function _80_()
        local code0 = nil
        if init then
          code0 = (byte - init["min-byte"])
        else
          code0 = nil
        end
        for i = (index + 1), (index + init.len + -1) do
          local byte0 = string.byte(str0, i)
          code0 = (byte0 and code0 and ((128 <= byte0) and (byte0 <= 191)) and ((code0 * 64) + (byte0 - 128)))
        end
        return code0
      end
      code = (init and _80_())
      if (code and (function(_82_,_83_,_84_) return (_82_ <= _83_) and (_83_ <= _84_) end)(init["min-code"],code,init["max-code"]) and not ((55296 <= code) and (code <= 57343))) then
        return init.len
      end
    end
    local index = 1
    local output = {}
    local byte_escape = (getopt(options, "byte-escape") or default_byte_escape)
    while (index <= #str) do
      local nexti = (string.find(str, "[\128-\255]", index) or (#str + 1))
      local len = validate_utf8(str, nexti)
      table.insert(output, string.sub(str, index, (nexti + (len or 0) + -1)))
      if (not len and (nexti <= #str)) then
        table.insert(output, byte_escape(str:byte(nexti), options))
      end
      if len then
        index = (nexti + len)
      else
        index = (nexti + 1)
      end
    end
    return table.concat(output)
  end
  local function pp_string(str, options, indent)
    local len = length_2a(str)
    local esc_newline_3f = ((len < 2) or (getopt(options, "escape-newlines?") and (len < (options["line-length"] - indent))))
    local byte_escape = (getopt(options, "byte-escape") or default_byte_escape)
    local escs = nil
    local _88_
    if esc_newline_3f then
      _88_ = "\\n"
    else
      _88_ = "\n"
    end
    local function _90_(_241, _242)
      return byte_escape(_242:byte(), options)
    end
    escs = setmetatable({["\""] = "\\\"", ["\11"] = "\\v", ["\12"] = "\\f", ["\13"] = "\\r", ["\7"] = "\\a", ["\8"] = "\\b", ["\9"] = "\\t", ["\\"] = "\\\\", ["\n"] = _88_}, {__index = _90_})
    local str0 = ("\"" .. str:gsub("[%c\\\"]", escs) .. "\"")
    if getopt(options, "utf8?") then
      return utf8_escape(str0, options)
    else
      return str0
    end
  end
  local function make_options(t, options)
    local defaults = nil
    do
      local tbl_14_ = {}
      for k, v in pairs(default_opts) do
        local k_15_, v_16_ = k, v
        if ((k_15_ ~= nil) and (v_16_ ~= nil)) then
          tbl_14_[k_15_] = v_16_
        end
      end
      defaults = tbl_14_
    end
    local overrides = {appearances = count_table_appearances(t, {}), level = 0, seen = {len = 0}}
    for k, v in pairs((options or {})) do
      defaults[k] = v
    end
    for k, v in pairs(overrides) do
      defaults[k] = v
    end
    return defaults
  end
  local function _93_(x, options, indent, colon_3f)
    local indent0 = (indent or 0)
    local options0 = (options or make_options(x))
    local x0 = nil
    if options0.preprocess then
      x0 = options0.preprocess(x, options0)
    else
      x0 = x
    end
    local tv = type(x0)
    local function _96_()
      local _95_0 = getmetatable(x0)
      if ((_G.type(_95_0) == "table") and true) then
        local __fennelview = _95_0.__fennelview
        return __fennelview
      end
    end
    if ((tv == "table") or ((tv == "userdata") and _96_())) then
      return pp_table(x0, options0, indent0)
    elseif (tv == "number") then
      return number__3estring(x0)
    else
      local function _98_()
        if (colon_3f ~= nil) then
          return colon_3f
        elseif ("function" == type(options0["prefer-colon?"])) then
          return options0["prefer-colon?"](x0)
        else
          return getopt(options0, "prefer-colon?")
        end
      end
      if ((tv == "string") and colon_string_3f(x0) and _98_()) then
        return (":" .. x0)
      elseif (tv == "string") then
        return pp_string(x0, options0, indent0)
      elseif ((tv == "boolean") or (tv == "nil")) then
        return tostring(x0)
      else
        return ("#<" .. tostring(x0) .. ">")
      end
    end
  end
  pp = _93_
  local function _view(x, _3foptions)
    return pp(x, make_options(x, _3foptions), 0)
  end
  return _view
end
package.preload["fennel.utils"] = package.preload["fennel.utils"] or function(...)
  local view = require("fennel.view")
  local version = "1.5.0"
  local function luajit_vm_3f()
    return ((nil ~= _G.jit) and (type(_G.jit) == "table") and (nil ~= _G.jit.on) and (nil ~= _G.jit.off) and (type(_G.jit.version_num) == "number"))
  end
  local function luajit_vm_version()
    local jit_os = nil
    if (_G.jit.os == "OSX") then
      jit_os = "macOS"
    else
      jit_os = _G.jit.os
    end
    return (_G.jit.version .. " " .. jit_os .. "/" .. _G.jit.arch)
  end
  local function fengari_vm_3f()
    return ((nil ~= _G.fengari) and (type(_G.fengari) == "table") and (nil ~= _G.fengari.VERSION) and (type(_G.fengari.VERSION_NUM) == "number"))
  end
  local function fengari_vm_version()
    return (_G.fengari.RELEASE .. " (" .. _VERSION .. ")")
  end
  local function lua_vm_version()
    if luajit_vm_3f() then
      return luajit_vm_version()
    elseif fengari_vm_3f() then
      return fengari_vm_version()
    else
      return ("PUC " .. _VERSION)
    end
  end
  local function runtime_version(_3fas_table)
    if _3fas_table then
      return {fennel = version, lua = lua_vm_version()}
    else
      return ("Fennel " .. version .. " on " .. lua_vm_version())
    end
  end
  local len = nil
  do
    local _103_0, _104_0 = pcall(require, "utf8")
    if ((_103_0 == true) and (nil ~= _104_0)) then
      local utf8 = _104_0
      len = utf8.len
    else
      local _ = _103_0
      len = string.len
    end
  end
  local kv_order = {boolean = 2, number = 1, string = 3, table = 4}
  local function kv_compare(a, b)
    local _106_0, _107_0 = type(a), type(b)
    if (((_106_0 == "number") and (_107_0 == "number")) or ((_106_0 == "string") and (_107_0 == "string"))) then
      return (a < b)
    else
      local function _108_()
        local a_t = _106_0
        local b_t = _107_0
        return (a_t ~= b_t)
      end
      if (((nil ~= _106_0) and (nil ~= _107_0)) and _108_()) then
        local a_t = _106_0
        local b_t = _107_0
        return ((kv_order[a_t] or 5) < (kv_order[b_t] or 5))
      else
        local _ = _106_0
        return (tostring(a) < tostring(b))
      end
    end
  end
  local function add_stable_keys(succ, prev_key, src, _3fpred)
    local first = prev_key
    local last = nil
    do
      local prev = prev_key
      for _, k in ipairs(src) do
        if ((prev == k) or (succ[k] ~= nil) or (_3fpred and not _3fpred(k))) then
          prev = prev
        else
          if (first == nil) then
            first = k
            prev = k
          elseif (prev ~= nil) then
            succ[prev] = k
            prev = k
          else
            prev = k
          end
        end
      end
      last = prev
    end
    return succ, last, first
  end
  local function stablepairs(t)
    local mt_keys = nil
    do
      local _112_0 = getmetatable(t)
      if (nil ~= _112_0) then
        _112_0 = _112_0.keys
      end
      mt_keys = _112_0
    end
    local succ, prev, first_mt = nil, nil, nil
    local function _114_(_241)
      return t[_241]
    end
    succ, prev, first_mt = add_stable_keys({}, nil, (mt_keys or {}), _114_)
    local pairs_keys = nil
    do
      local _115_0 = nil
      do
        local tbl_17_ = {}
        local i_18_ = #tbl_17_
        for k in pairs(t) do
          local val_19_ = k
          if (nil ~= val_19_) then
            i_18_ = (i_18_ + 1)
            tbl_17_[i_18_] = val_19_
          end
        end
        _115_0 = tbl_17_
      end
      table.sort(_115_0, kv_compare)
      pairs_keys = _115_0
    end
    local succ0, _, first_after_mt = add_stable_keys(succ, prev, pairs_keys)
    local first = nil
    if (first_mt == nil) then
      first = first_after_mt
    else
      first = first_mt
    end
    local function stablenext(tbl, key)
      local _118_0 = nil
      if (key == nil) then
        _118_0 = first
      else
        _118_0 = succ0[key]
      end
      if (nil ~= _118_0) then
        local next_key = _118_0
        local _120_0 = tbl[next_key]
        if (_120_0 ~= nil) then
          return next_key, _120_0
        else
          return _120_0
        end
      end
    end
    return stablenext, t, nil
  end
  local function get_in(tbl, path)
    if (nil ~= path[1]) then
      local t = tbl
      for _, k in ipairs(path) do
        if (nil == t) then break end
        if (type(t) == "table") then
          t = t[k]
        else
        t = nil
        end
      end
      return t
    end
  end
  local function copy(_3ffrom, _3fto)
    local tbl_14_ = (_3fto or {})
    for k, v in pairs((_3ffrom or {})) do
      local k_15_, v_16_ = k, v
      if ((k_15_ ~= nil) and (v_16_ ~= nil)) then
        tbl_14_[k_15_] = v_16_
      end
    end
    return tbl_14_
  end
  local function member_3f(x, tbl, _3fn)
    local _126_0 = tbl[(_3fn or 1)]
    if (_126_0 == x) then
      return true
    elseif (_126_0 == nil) then
      return nil
    else
      local _ = _126_0
      return member_3f(x, tbl, ((_3fn or 1) + 1))
    end
  end
  local function maxn(tbl)
    local max = 0
    for k in pairs(tbl) do
      if ("number" == type(k)) then
        max = math.max(max, k)
      else
        max = max
      end
    end
    return max
  end
  local function every_3f(t, predicate)
    local result = true
    for _, item in ipairs(t) do
      if not result then break end
      result = predicate(item)
    end
    return result
  end
  local function allpairs(tbl)
    assert((type(tbl) == "table"), "allpairs expects a table")
    local t = tbl
    local seen = {}
    local function allpairs_next(_, state)
      local next_state, value = next(t, state)
      if seen[next_state] then
        return allpairs_next(nil, next_state)
      elseif next_state then
        seen[next_state] = true
        return next_state, value
      else
        local _129_0 = getmetatable(t)
        if ((_G.type(_129_0) == "table") and true) then
          local __index = _129_0.__index
          if ("table" == type(__index)) then
            t = __index
            return allpairs_next(t)
          end
        end
      end
    end
    return allpairs_next
  end
  local function deref(self)
    return self[1]
  end
  local function list__3estring(self, _3fview, _3foptions, _3findent)
    local viewed = nil
    do
      local tbl_17_ = {}
      local i_18_ = #tbl_17_
      for i = 1, maxn(self) do
        local val_19_ = nil
        if _3fview then
          val_19_ = _3fview(self[i], _3foptions, _3findent)
        else
          val_19_ = view(self[i])
        end
        if (nil ~= val_19_) then
          i_18_ = (i_18_ + 1)
          tbl_17_[i_18_] = val_19_
        end
      end
      viewed = tbl_17_
    end
    return ("(" .. table.concat(viewed, " ") .. ")")
  end
  local function comment_view(c)
    return c, true
  end
  local function sym_3d(a, b)
    return ((deref(a) == deref(b)) and (getmetatable(a) == getmetatable(b)))
  end
  local function sym_3c(a, b)
    return (a[1] < tostring(b))
  end
  local symbol_mt = {"SYMBOL", __eq = sym_3d, __fennelview = deref, __lt = sym_3c, __tostring = deref}
  local expr_mt = nil
  local function _135_(x)
    return tostring(deref(x))
  end
  expr_mt = {"EXPR", __tostring = _135_}
  local list_mt = {"LIST", __fennelview = list__3estring, __tostring = list__3estring}
  local comment_mt = {"COMMENT", __eq = sym_3d, __fennelview = comment_view, __lt = sym_3c, __tostring = deref}
  local sequence_marker = {"SEQUENCE"}
  local varg_mt = {"VARARG", __fennelview = deref, __tostring = deref}
  local getenv = nil
  local function _136_()
    return nil
  end
  getenv = ((os and os.getenv) or _136_)
  local function debug_on_3f(flag)
    local level = (getenv("FENNEL_DEBUG") or "")
    return ((level == "all") or level:find(flag))
  end
  local function list(...)
    return setmetatable({...}, list_mt)
  end
  local function sym(str, _3fsource)
    local _137_
    do
      local tbl_14_ = {str}
      for k, v in pairs((_3fsource or {})) do
        local k_15_, v_16_ = nil, nil
        if (type(k) == "string") then
          k_15_, v_16_ = k, v
        else
        k_15_, v_16_ = nil
        end
        if ((k_15_ ~= nil) and (v_16_ ~= nil)) then
          tbl_14_[k_15_] = v_16_
        end
      end
      _137_ = tbl_14_
    end
    return setmetatable(_137_, symbol_mt)
  end
  local function sequence(...)
    local function _140_(seq, view0, inspector, indent)
      local opts = nil
      do
        inspector["empty-as-sequence?"] = {after = inspector["empty-as-sequence?"], once = true}
        inspector["metamethod?"] = {after = inspector["metamethod?"], once = false}
        opts = inspector
      end
      return view0(seq, opts, indent)
    end
    return setmetatable({...}, {__fennelview = _140_, sequence = sequence_marker})
  end
  local function expr(strcode, etype)
    return setmetatable({strcode, type = etype}, expr_mt)
  end
  local function comment_2a(contents, _3fsource)
    local _141_ = (_3fsource or {})
    local filename = _141_["filename"]
    local line = _141_["line"]
    return setmetatable({contents, filename = filename, line = line}, comment_mt)
  end
  local function varg(_3fsource)
    local _142_
    do
      local tbl_14_ = {"..."}
      for k, v in pairs((_3fsource or {})) do
        local k_15_, v_16_ = nil, nil
        if (type(k) == "string") then
          k_15_, v_16_ = k, v
        else
        k_15_, v_16_ = nil
        end
        if ((k_15_ ~= nil) and (v_16_ ~= nil)) then
          tbl_14_[k_15_] = v_16_
        end
      end
      _142_ = tbl_14_
    end
    return setmetatable(_142_, varg_mt)
  end
  local function expr_3f(x)
    return ((type(x) == "table") and (getmetatable(x) == expr_mt) and x)
  end
  local function varg_3f(x)
    return ((type(x) == "table") and (getmetatable(x) == varg_mt) and x)
  end
  local function list_3f(x)
    return ((type(x) == "table") and (getmetatable(x) == list_mt) and x)
  end
  local function sym_3f(x, _3fname)
    return ((type(x) == "table") and (getmetatable(x) == symbol_mt) and ((nil == _3fname) or (x[1] == _3fname)) and x)
  end
  local function sequence_3f(x)
    local mt = ((type(x) == "table") and getmetatable(x))
    return (mt and (mt.sequence == sequence_marker) and x)
  end
  local function comment_3f(x)
    return ((type(x) == "table") and (getmetatable(x) == comment_mt) and x)
  end
  local function table_3f(x)
    return ((type(x) == "table") and not varg_3f(x) and (getmetatable(x) ~= list_mt) and (getmetatable(x) ~= symbol_mt) and not comment_3f(x) and x)
  end
  local function kv_table_3f(t)
    if table_3f(t) then
      local nxt, t0, k = pairs(t)
      local len0 = #t0
      local next_state = nil
      if (0 == len0) then
        next_state = k
      else
        next_state = len0
      end
      return ((nil ~= nxt(t0, next_state)) and t0)
    end
  end
  local function string_3f(x)
    if (type(x) == "string") then
      return x
    else
      return false
    end
  end
  local function multi_sym_3f(str)
    if sym_3f(str) then
      return multi_sym_3f(tostring(str))
    elseif (type(str) ~= "string") then
      return false
    else
      local function _148_()
        local parts = {}
        for part in str:gmatch("[^%.%:]+[%.%:]?") do
          local last_char = part:sub(-1)
          if (last_char == ":") then
            parts["multi-sym-method-call"] = true
          end
          if ((last_char == ":") or (last_char == ".")) then
            parts[(#parts + 1)] = part:sub(1, -2)
          else
            parts[(#parts + 1)] = part
          end
        end
        return (next(parts) and parts)
      end
      return ((str:match("%.") or str:match(":")) and not str:match("%.%.") and (str:byte() ~= string.byte(".")) and (str:byte() ~= string.byte(":")) and (str:byte(-1) ~= string.byte(".")) and (str:byte(-1) ~= string.byte(":")) and _148_())
    end
  end
  local function call_of_3f(ast, callee)
    return (list_3f(ast) and sym_3f(ast[1], callee))
  end
  local function quoted_3f(symbol)
    return symbol.quoted
  end
  local function idempotent_expr_3f(x)
    local t = type(x)
    return ((t == "string") or (t == "number") or (t == "boolean") or (sym_3f(x) and not multi_sym_3f(x)))
  end
  local function walk_tree(root, f, _3fcustom_iterator)
    local function walk(iterfn, parent, idx, node)
      if (f(idx, node, parent) and not sym_3f(node)) then
        for k, v in iterfn(node) do
          walk(iterfn, node, k, v)
        end
        return nil
      end
    end
    walk((_3fcustom_iterator or pairs), nil, nil, root)
    return root
  end
  local root = nil
  local function _153_()
  end
  root = {chunk = nil, options = nil, reset = _153_, scope = nil}
  root["set-reset"] = function(_154_0)
    local _155_ = _154_0
    local chunk = _155_["chunk"]
    local options = _155_["options"]
    local reset = _155_["reset"]
    local scope = _155_["scope"]
    root.reset = function()
      root.chunk, root.scope, root.options, root.reset = chunk, scope, options, reset
      return nil
    end
    return root.reset
  end
  local lua_keywords = {["and"] = true, ["break"] = true, ["do"] = true, ["else"] = true, ["elseif"] = true, ["end"] = true, ["false"] = true, ["for"] = true, ["function"] = true, ["goto"] = true, ["if"] = true, ["in"] = true, ["local"] = true, ["nil"] = true, ["not"] = true, ["or"] = true, ["repeat"] = true, ["return"] = true, ["then"] = true, ["true"] = true, ["until"] = true, ["while"] = true}
  local function lua_keyword_3f(str)
    local function _157_()
      local _156_0 = root.options
      if (nil ~= _156_0) then
        _156_0 = _156_0.keywords
      end
      if (nil ~= _156_0) then
        _156_0 = _156_0[str]
      end
      return _156_0
    end
    return (lua_keywords[str] or _157_())
  end
  local function valid_lua_identifier_3f(str)
    return (str:match("^[%a_][%w_]*$") and not lua_keyword_3f(str))
  end
  local propagated_options = {"allowedGlobals", "indent", "correlate", "useMetadata", "env", "compiler-env", "compilerEnv"}
  local function propagate_options(options, subopts)
    local tbl_14_ = subopts
    for _, name in ipairs(propagated_options) do
      local k_15_, v_16_ = name, options[name]
      if ((k_15_ ~= nil) and (v_16_ ~= nil)) then
        tbl_14_[k_15_] = v_16_
      end
    end
    return tbl_14_
  end
  local function ast_source(ast)
    if (table_3f(ast) or sequence_3f(ast)) then
      return (getmetatable(ast) or {})
    elseif ("table" == type(ast)) then
      return ast
    else
      return {}
    end
  end
  local function warn(msg, _3fast, _3ffilename, _3fline)
    if (_G.io and _G.io.stderr) then
      local loc = nil
      do
        local _162_0 = ast_source(_3fast)
        if ((_G.type(_162_0) == "table") and (nil ~= _162_0.filename) and (nil ~= _162_0.line)) then
          local filename = _162_0.filename
          local line = _162_0.line
          loc = (filename .. ":" .. line .. ": ")
        else
          local _ = _162_0
          if (_3ffilename and _3fline) then
            loc = (_3ffilename .. ":" .. _3fline .. ": ")
          else
            loc = ""
          end
        end
      end
      return (_G.io.stderr):write(("--WARNING: %s%s\n"):format(loc, tostring(msg)))
    end
  end
  local warned = {}
  local function check_plugin_version(_166_0)
    local _167_ = _166_0
    local plugin = _167_
    local name = _167_["name"]
    local versions = _167_["versions"]
    if (not member_3f(version:gsub("-dev", ""), (versions or {})) and not (string_3f(versions) and version:find(versions)) and not warned[plugin]) then
      warned[plugin] = true
      return warn(string.format("plugin %s does not support Fennel version %s", (name or "unknown"), version))
    end
  end
  local function hook_opts(event, _3foptions, ...)
    local plugins = nil
    local function _170_(...)
      local _169_0 = _3foptions
      if (nil ~= _169_0) then
        _169_0 = _169_0.plugins
      end
      return _169_0
    end
    local function _173_(...)
      local _172_0 = root.options
      if (nil ~= _172_0) then
        _172_0 = _172_0.plugins
      end
      return _172_0
    end
    plugins = (_170_(...) or _173_(...))
    if plugins then
      local result = nil
      for _, plugin in ipairs(plugins) do
        if (nil ~= result) then break end
        check_plugin_version(plugin)
        local _175_0 = plugin[event]
        if (nil ~= _175_0) then
          local f = _175_0
          result = f(...)
        else
        result = nil
        end
      end
      return result
    end
  end
  local function hook(event, ...)
    return hook_opts(event, root.options, ...)
  end
  return {["ast-source"] = ast_source, ["call-of?"] = call_of_3f, ["comment?"] = comment_3f, ["debug-on?"] = debug_on_3f, ["every?"] = every_3f, ["expr?"] = expr_3f, ["fennel-module"] = nil, ["get-in"] = get_in, ["hook-opts"] = hook_opts, ["idempotent-expr?"] = idempotent_expr_3f, ["kv-table?"] = kv_table_3f, ["list?"] = list_3f, ["lua-keyword?"] = lua_keyword_3f, ["macro-path"] = table.concat({"./?.fnl", "./?/init-macros.fnl", "./?/init.fnl", getenv("FENNEL_MACRO_PATH")}, ";"), ["member?"] = member_3f, ["multi-sym?"] = multi_sym_3f, ["propagate-options"] = propagate_options, ["quoted?"] = quoted_3f, ["runtime-version"] = runtime_version, ["sequence?"] = sequence_3f, ["string?"] = string_3f, ["sym?"] = sym_3f, ["table?"] = table_3f, ["valid-lua-identifier?"] = valid_lua_identifier_3f, ["varg?"] = varg_3f, ["walk-tree"] = walk_tree, allpairs = allpairs, comment = comment_2a, copy = copy, expr = expr, hook = hook, len = len, list = list, maxn = maxn, path = table.concat({"./?.fnl", "./?/init.fnl", getenv("FENNEL_PATH")}, ";"), root = root, sequence = sequence, stablepairs = stablepairs, sym = sym, varg = varg, version = version, warn = warn}
end
utils = require("fennel.utils")
local parser = require("fennel.parser")
local compiler = require("fennel.compiler")
local specials = require("fennel.specials")
local repl = require("fennel.repl")
local view = require("fennel.view")
local function eval_env(env, opts)
  if (env == "_COMPILER") then
    local env0 = specials["make-compiler-env"](nil, compiler.scopes.compiler, {}, opts)
    if (opts.allowedGlobals == nil) then
      opts.allowedGlobals = specials["current-global-names"](env0)
    end
    return specials["wrap-env"](env0)
  else
    return (env and specials["wrap-env"](env))
  end
end
local function eval_opts(options, str)
  local opts = utils.copy(options)
  if (opts.allowedGlobals == nil) then
    opts.allowedGlobals = specials["current-global-names"](opts.env)
  end
  if (not opts.filename and not opts.source) then
    opts.source = str
  end
  if (opts.env == "_COMPILER") then
    opts.scope = compiler["make-scope"](compiler.scopes.compiler)
  end
  return opts
end
local function eval(str, _3foptions, ...)
  local opts = eval_opts(_3foptions, str)
  local env = eval_env(opts.env, opts)
  local lua_source = compiler["compile-string"](str, opts)
  local loader = nil
  local function _814_(...)
    if opts.filename then
      return ("@" .. opts.filename)
    else
      return str
    end
  end
  loader = specials["load-code"](lua_source, env, _814_(...))
  opts.filename = nil
  return loader(...)
end
local function dofile_2a(filename, _3foptions, ...)
  local opts = utils.copy(_3foptions)
  local f = assert(io.open(filename, "rb"))
  local source = assert(f:read("*all"), ("Could not read " .. filename))
  f:close()
  opts.filename = filename
  return eval(source, opts, ...)
end
local function syntax()
  local body_3f = {"when", "with-open", "collect", "icollect", "fcollect", "lambda", "\206\187", "macro", "match", "match-try", "case", "case-try", "accumulate", "faccumulate", "doto"}
  local binding_3f = {"collect", "icollect", "fcollect", "each", "for", "let", "with-open", "accumulate", "faccumulate"}
  local define_3f = {"fn", "lambda", "\206\187", "var", "local", "macro", "macros", "global"}
  local deprecated = {"~=", "#", "global", "require-macros", "pick-args"}
  local out = {}
  for k, v in pairs(compiler.scopes.global.specials) do
    local metadata = (compiler.metadata[v] or {})
    out[k] = {["binding-form?"] = utils["member?"](k, binding_3f), ["body-form?"] = metadata["fnl/body-form?"], ["define?"] = utils["member?"](k, define_3f), ["deprecated?"] = utils["member?"](k, deprecated), ["special?"] = true}
  end
  for k in pairs(compiler.scopes.global.macros) do
    out[k] = {["binding-form?"] = utils["member?"](k, binding_3f), ["body-form?"] = utils["member?"](k, body_3f), ["define?"] = utils["member?"](k, define_3f), ["macro?"] = true}
  end
  for k, v in pairs(_G) do
    local _815_0 = type(v)
    if (_815_0 == "function") then
      out[k] = {["function?"] = true, ["global?"] = true}
    elseif (_815_0 == "table") then
      if not k:find("^_") then
        for k2, v2 in pairs(v) do
          if ("function" == type(v2)) then
            out[(k .. "." .. k2)] = {["function?"] = true, ["global?"] = true}
          end
        end
        out[k] = {["global?"] = true}
      end
    end
  end
  return out
end
local mod = {["ast-source"] = utils["ast-source"], ["comment?"] = utils["comment?"], ["compile-stream"] = compiler["compile-stream"], ["compile-string"] = compiler["compile-string"], ["list?"] = utils["list?"], ["load-code"] = specials["load-code"], ["macro-loaded"] = specials["macro-loaded"], ["macro-path"] = utils["macro-path"], ["macro-searchers"] = specials["macro-searchers"], ["make-searcher"] = specials["make-searcher"], ["multi-sym?"] = utils["multi-sym?"], ["runtime-version"] = utils["runtime-version"], ["search-module"] = specials["search-module"], ["sequence?"] = utils["sequence?"], ["string-stream"] = parser["string-stream"], ["sym-char?"] = parser["sym-char?"], ["sym?"] = utils["sym?"], ["table?"] = utils["table?"], ["varg?"] = utils["varg?"], comment = utils.comment, compile = compiler.compile, compile1 = compiler.compile1, compileStream = compiler["compile-stream"], compileString = compiler["compile-string"], doc = specials.doc, dofile = dofile_2a, eval = eval, gensym = compiler.gensym, getinfo = compiler.getinfo, granulate = parser.granulate, list = utils.list, loadCode = specials["load-code"], macroLoaded = specials["macro-loaded"], macroPath = utils["macro-path"], macroSearchers = specials["macro-searchers"], makeSearcher = specials["make-searcher"], make_searcher = specials["make-searcher"], mangle = compiler["global-mangling"], metadata = compiler.metadata, parser = parser.parser, path = utils.path, repl = repl, runtimeVersion = utils["runtime-version"], scope = compiler["make-scope"], searchModule = specials["search-module"], searcher = specials["make-searcher"](), sequence = utils.sequence, stringStream = parser["string-stream"], sym = utils.sym, syntax = syntax, traceback = compiler.traceback, unmangle = compiler["global-unmangling"], varg = utils.varg, version = utils.version, view = view}
mod.install = function(_3fopts)
  table.insert((package.searchers or package.loaders), specials["make-searcher"](_3fopts))
  return mod
end
utils["fennel-module"] = mod
do
  local module_name = "fennel.macros"
  local _ = nil
  local function _819_()
    return mod
  end
  package.preload[module_name] = _819_
  _ = nil
  local env = nil
  do
    local _820_0 = specials["make-compiler-env"](nil, compiler.scopes.compiler, {})
    _820_0["utils"] = utils
    _820_0["fennel"] = mod
    _820_0["get-function-metadata"] = specials["get-function-metadata"]
    env = _820_0
  end
  local built_ins = eval([===[;; fennel-ls: macro-file
  
  ;; These macros are awkward because their definition cannot rely on the any
  ;; built-in macros, only special forms. (no when, no icollect, etc)
  
  (fn copy [t]
    (let [out []]
      (each [_ v (ipairs t)] (table.insert out v))
      (setmetatable out (getmetatable t))))
  
  (fn ->* [val ...]
    "Thread-first macro.
  Take the first value and splice it into the second form as its first argument.
  The value of the second form is spliced into the first arg of the third, etc."
    (var x val)
    (each [_ e (ipairs [...])]
      (let [elt (if (list? e) (copy e) (list e))]
        (table.insert elt 2 x)
        (set x elt)))
    x)
  
  (fn ->>* [val ...]
    "Thread-last macro.
  Same as ->, except splices the value into the last position of each form
  rather than the first."
    (var x val)
    (each [_ e (ipairs [...])]
      (let [elt (if (list? e) (copy e) (list e))]
        (table.insert elt x)
        (set x elt)))
    x)
  
  (fn -?>* [val ?e ...]
    "Nil-safe thread-first macro.
  Same as -> except will short-circuit with nil when it encounters a nil value."
    (if (= nil ?e)
        val
        (not (utils.idempotent-expr? val))
        ;; try again, but with an eval-safe val
        `(let [tmp# ,val]
          (-?> tmp# ,?e ,...))
        (let [call (if (list? ?e) (copy ?e) (list ?e))]
          (table.insert call 2 val)
          `(if (not= nil ,val)
               ,(-?>* call ...)))))
  
  (fn -?>>* [val ?e ...]
    "Nil-safe thread-last macro.
  Same as ->> except will short-circuit with nil when it encounters a nil value."
    (if (= nil ?e)
        val
        (not (utils.idempotent-expr? val))
        ;; try again, but with an eval-safe val
        `(let [tmp# ,val]
          (-?>> tmp# ,?e ,...))
        (let [call (if (list? ?e) (copy ?e) (list ?e))]
          (table.insert call val)
          `(if (not= ,val nil)
               ,(-?>>* call ...)))))
  
  (fn ?dot [tbl ...]
    "Nil-safe table look up.
  Same as . (dot), except will short-circuit with nil when it encounters
  a nil value in any of subsequent keys."
    (let [head (gensym :t)
          lookups `(do
                     (var ,head ,tbl)
                     ,head)]
      (each [i k (ipairs [...])]
        ;; Kinda gnarly to reassign in place like this, but it emits the best lua.
        ;; With this impl, it emits a flat, concise, and readable set of ifs
        (table.insert lookups (+ i 2)
                      `(if (not= nil ,head) (set ,head (. ,head ,k)))))
      lookups))
  
  (fn doto* [val ...]
    "Evaluate val and splice it into the first argument of subsequent forms."
    (assert (not= val nil) "missing subject")
    (if (not (utils.idempotent-expr? val))
      `(let [tmp# ,val]
         (doto tmp# ,...))
      (let [form `(do)]
        (each [_ elt (ipairs [...])]
          (let [elt (if (list? elt) (copy elt) (list elt))]
            (table.insert elt 2 val)
            (table.insert form elt)))
        (table.insert form val)
        form)))
  
  (fn when* [condition body1 ...]
    "Evaluate body for side-effects only when condition is truthy."
    (assert body1 "expected body")
    `(if ,condition
         (do
           ,body1
           ,...)))
  
  (fn with-open* [closable-bindings ...]
    "Like `let`, but invokes (v:close) on each binding after evaluating the body.
  The body is evaluated inside `xpcall` so that bound values will be closed upon
  encountering an error before propagating it."
    (let [bodyfn `(fn []
                    ,...)
          closer `(fn close-handlers# [ok# ...]
                    (if ok# ... (error ... 0)))
          traceback `(. (or (. package.loaded ,(fennel-module-name)) _G.debug {})
                        :traceback)]
      (for [i 1 (length closable-bindings) 2]
        (assert (sym? (. closable-bindings i))
                "with-open only allows symbols in bindings")
        (table.insert closer 4 `(: ,(. closable-bindings i) :close)))
      `(let ,closable-bindings
         ,closer
         (close-handlers# (_G.xpcall ,bodyfn ,traceback)))))
  
  (fn extract-into [iter-tbl]
    (var (into iter-out found?) (values [] (copy iter-tbl)))
    (for [i (length iter-tbl) 2 -1]
      (let [item (. iter-tbl i)]
        (if (or (sym? item "&into") (= :into item))
            (do
              (assert (not found?) "expected only one &into clause")
              (set found? true)
              (set into (. iter-tbl (+ i 1)))
              (table.remove iter-out i)
              (table.remove iter-out i)))))
    (assert (or (not found?) (sym? into) (table? into) (list? into))
            "expected table, function call, or symbol in &into clause")
    (values into iter-out found?))
  
  (fn collect* [iter-tbl key-expr value-expr ...]
    "Return a table made by running an iterator and evaluating an expression that
  returns key-value pairs to be inserted sequentially into the table.  This can
  be thought of as a table comprehension. The body should provide two expressions
  (used as key and value) or nil, which causes it to be omitted.
  
  For example,
    (collect [k v (pairs {:apple \"red\" :orange \"orange\"})]
      (values v k))
  returns
    {:red \"apple\" :orange \"orange\"}
  
  Supports an &into clause after the iterator to put results in an existing table.
  Supports early termination with an &until clause."
    (assert (and (sequence? iter-tbl) (<= 2 (length iter-tbl)))
            "expected iterator binding table")
    (assert (not= nil key-expr) "expected key and value expression")
    (assert (= nil ...)
            "expected 1 or 2 body expressions; wrap multiple expressions with do")
    (assert (or value-expr (list? key-expr)) "need key and value")
    (let [kv-expr (if (= nil value-expr) key-expr `(values ,key-expr ,value-expr))
          (into iter) (extract-into iter-tbl)]
      `(let [tbl# ,into]
         (each ,iter
           (let [(k# v#) ,kv-expr]
             (if (and (not= k# nil) (not= v# nil))
               (tset tbl# k# v#))))
         tbl#)))
  
  (fn seq-collect [how iter-tbl value-expr ...]
    "Common part between icollect and fcollect for producing sequential tables.
  
  Iteration code only differs in using the for or each keyword, the rest
  of the generated code is identical."
    (assert (not= nil value-expr) "expected table value expression")
    (assert (= nil ...)
            "expected exactly one body expression. Wrap multiple expressions in do")
    (let [(into iter has-into?) (extract-into iter-tbl)]
      (if has-into?
          `(let [tbl# ,into]
             (,how ,iter (let [val# ,value-expr]
                           (table.insert tbl# val#)))
             tbl#)
          ;; believe it or not, using a var here has a pretty good performance
          ;; boost: https://p.hagelb.org/icollect-performance.html
          ;; but it doesn't always work with &into clauses, so skip if that's used
          `(let [tbl# []]
             (var i# 0)
             (,how ,iter
                   (let [val# ,value-expr]
                     (when (not= nil val#)
                       (set i# (+ i# 1))
                       (tset tbl# i# val#))))
             tbl#))))
  
  (fn icollect* [iter-tbl value-expr ...]
    "Return a sequential table made by running an iterator and evaluating an
  expression that returns values to be inserted sequentially into the table.
  This can be thought of as a table comprehension. If the body evaluates to nil
  that element is omitted.
  
  For example,
    (icollect [_ v (ipairs [1 2 3 4 5])]
      (when (not= v 3)
        (* v v)))
  returns
    [1 4 16 25]
  
  Supports an &into clause after the iterator to put results in an existing table.
  Supports early termination with an &until clause."
    (assert (and (sequence? iter-tbl) (<= 2 (length iter-tbl)))
            "expected iterator binding table")
    (seq-collect 'each iter-tbl value-expr ...))
  
  (fn fcollect* [iter-tbl value-expr ...]
    "Return a sequential table made by advancing a range as specified by
  for, and evaluating an expression that returns values to be inserted
  sequentially into the table.  This can be thought of as a range
  comprehension. If the body evaluates to nil that element is omitted.
  
  For example,
    (fcollect [i 1 10 2]
      (when (not= i 3)
        (* i i)))
  returns
    [1 25 49 81]
  
  Supports an &into clause after the range to put results in an existing table.
  Supports early termination with an &until clause."
    (assert (and (sequence? iter-tbl) (< 2 (length iter-tbl)))
            "expected range binding table")
    (seq-collect 'for iter-tbl value-expr ...))
  
  (fn accumulate-impl [for? iter-tbl body ...]
    (assert (and (sequence? iter-tbl) (<= 4 (length iter-tbl)))
            "expected initial value and iterator binding table")
    (assert (not= nil body) "expected body expression")
    (assert (= nil ...)
            "expected exactly one body expression. Wrap multiple expressions with do")
    (let [[accum-var accum-init] iter-tbl
          iter (sym (if for? "for" "each"))] ; accumulate or faccumulate?
      `(do
         (var ,accum-var ,accum-init)
         (,iter ,[(unpack iter-tbl 3)]
                (set ,accum-var ,body))
         ,(if (list? accum-var)
            (list (sym :values) (unpack accum-var))
            accum-var))))
  
  (fn accumulate* [iter-tbl body ...]
    "Accumulation macro.
  
  It takes a binding table and an expression as its arguments.  In the binding
  table, the first form starts out bound to the second value, which is an initial
  accumulator. The rest are an iterator binding table in the format `each` takes.
  
  It runs through the iterator in each step of which the given expression is
  evaluated, and the accumulator is set to the value of the expression. It
  eventually returns the final value of the accumulator.
  
  For example,
    (accumulate [total 0
                 _ n (pairs {:apple 2 :orange 3})]
      (+ total n))
  returns 5"
    (accumulate-impl false iter-tbl body ...))
  
  (fn faccumulate* [iter-tbl body ...]
    "Identical to accumulate, but after the accumulator the binding table is the
  same as `for` instead of `each`. Like collect to fcollect, will iterate over a
  numerical range like `for` rather than an iterator."
    (accumulate-impl true iter-tbl body ...))
  
  (fn partial* [f ...]
    "Return a function with all arguments partially applied to f."
    (assert f "expected a function to partially apply")
    (let [bindings []
          args []]
      (each [_ arg (ipairs [...])]
        (if (utils.idempotent-expr? arg)
          (table.insert args arg)
          (let [name (gensym)]
            (table.insert bindings name)
            (table.insert bindings arg)
            (table.insert args name))))
      (let [body (list f (unpack args))]
        (table.insert body _VARARG)
        ;; only use the extra let if we need double-eval protection
        (if (= nil (. bindings 1))
            `(fn [,_VARARG] ,body)
            `(let ,bindings
               (fn [,_VARARG] ,body))))))
  
  (fn pick-args* [n f]
    "Create a function of arity n that applies its arguments to f. Deprecated."
    (if (and _G.io _G.io.stderr)
        (_G.io.stderr:write
         "-- WARNING: pick-args is deprecated and will be removed in the future.\n"))
    (let [bindings []]
      (for [i 1 n] (tset bindings i (gensym)))
      `(fn ,bindings (,f ,(unpack bindings)))))
  
  (fn lambda* [...]
    "Function literal with nil-checked arguments.
  Like `fn`, but will throw an exception if a declared argument is passed in as
  nil, unless that argument's name begins with a question mark."
    (let [args [...]
          args-len (length args)
          has-internal-name? (sym? (. args 1))
          arglist (if has-internal-name? (. args 2) (. args 1))
          metadata-position (if has-internal-name? 3 2)
          (_ check-position) (get-function-metadata [:lambda ...] arglist
                                                    metadata-position)
          empty-body? (< args-len check-position)]
      (fn check! [a]
        (if (table? a)
            (each [_ a (pairs a)] (check! a))
            (let [as (tostring a)]
              (and (not (as:find "^?")) (not= as "&") (not (as:find "^_"))
                   (not= as "...") (not= as "&as")))
            (table.insert args check-position
                          `(_G.assert (not= nil ,a)
                                      ,(: "Missing argument %s on %s:%s" :format
                                          (tostring a)
                                          (or a.filename :unknown)
                                          (or a.line "?"))))))
  
      (assert (= :table (type arglist)) "expected arg list")
      (each [_ a (ipairs arglist)] (check! a))
      (if empty-body? (table.insert args (sym :nil)))
      `(fn ,(unpack args))))
  
  (fn macro* [name ...]
    "Define a single macro."
    (assert (sym? name) "expected symbol for macro name")
    (local args [...])
    `(macros {,(tostring name) (fn ,(unpack args))}))
  
  (fn macrodebug* [form return?]
    "Print the resulting form after performing macroexpansion.
  With a second argument, returns expanded form as a string instead of printing."
    (let [handle (if return? `do `print)]
      ;; TODO: Provide a helpful compiler error in the unlikely edge case of an
      ;; infinite AST instead of the current "silently expand until max depth"
      `(,handle ,(view (macroexpand form _SCOPE) {:detect-cycles? false}))))
  
  (fn import-macros* [binding1 module-name1 ...]
    "Bind a table of macros from each macro module according to a binding form.
  Each binding form can be either a symbol or a k/v destructuring table.
  Example:
    (import-macros mymacros                 :my-macros    ; bind to symbol
                   {:macro1 alias : macro2} :proj.macros) ; import by name"
    (assert (and binding1 module-name1 (= 0 (% (select "#" ...) 2)))
            "expected even number of binding/modulename pairs")
    (for [i 1 (select "#" binding1 module-name1 ...) 2]
      ;; delegate the actual loading of the macros to the require-macros
      ;; special which already knows how to set up the compiler env and stuff.
      ;; this is weird because require-macros is deprecated but it works.
      (let [(binding modname) (select i binding1 module-name1 ...)
            scope (get-scope)
            ;; if the module-name is an expression (and not just a string) we
            ;; patch our expression to have the correct source filename so
            ;; require-macros can pass it down when resolving the module-name.
            expr `(import-macros ,modname)
            filename (if (list? modname) (. modname 1 :filename) :unknown)
            _ (tset expr :filename filename)
            macros* (_SPECIALS.require-macros expr scope {} binding)]
        (if (sym? binding)
            ;; bind whole table of macros to table bound to symbol
            (tset scope.macros (. binding 1) macros*)
            ;; 1-level table destructuring for importing individual macros
            (table? binding)
            (each [macro-name [import-key] (pairs binding)]
              (assert (= :function (type (. macros* macro-name)))
                      (.. "macro " macro-name " not found in module "
                          (tostring modname)))
              (tset scope.macros import-key (. macros* macro-name))))))
    nil)
  
  (fn assert-repl* [condition ...]
    "Enter into a debug REPL  and print the message when condition is false/nil.
  Works as a drop-in replacement for Lua's `assert`.
  REPL `,return` command returns values to assert in place to continue execution."
    {:fnl/arglist [condition ?message ...]}
    (fn add-locals [{: symmeta : parent} locals]
      (each [name (pairs symmeta)]
        (tset locals name (sym name)))
      (if parent (add-locals parent locals) locals))
    `(let [unpack# (or table.unpack _G.unpack)
           pack# (or table.pack #(doto [$...] (tset :n (select :# $...))))
           ;; need to pack/unpack input args to account for (assert (foo)),
           ;; because assert returns *all* arguments upon success
           vals# (pack# ,condition ,...)
           condition# (. vals# 1)
           message# (or (. vals# 2) "assertion failed, entering repl.")]
       (if (not condition#)
           (let [opts# {:assert-repl? true}
                 fennel# (require ,(fennel-module-name))
                 locals# ,(add-locals (get-scope) [])]
             (set opts#.message (fennel#.traceback message#))
             (set opts#.env (collect [k# v# (pairs _G) &into locals#]
                              (if (= nil (. locals# k#)) (values k# v#))))
             (_G.assert (fennel#.repl opts#)))
           (values (unpack# vals# 1 vals#.n)))))
  
  {:-> ->*
   :->> ->>*
   :-?> -?>*
   :-?>> -?>>*
   :?. ?dot
   :doto doto*
   :when when*
   :with-open with-open*
   :collect collect*
   :icollect icollect*
   :fcollect fcollect*
   :accumulate accumulate*
   :faccumulate faccumulate*
   :partial partial*
   :lambda lambda*
   :λ lambda*
   :pick-args pick-args*
   :macro macro*
   :macrodebug macrodebug*
   :import-macros import-macros*
   :assert-repl assert-repl*}
  ]===], {env = env, filename = "src/fennel/macros.fnl", moduleName = module_name, scope = compiler.scopes.compiler, useMetadata = true})
  local _0 = nil
  for k, v in pairs(built_ins) do
    compiler.scopes.global.macros[k] = v
  end
  _0 = nil
  local match_macros = eval([===[;; fennel-ls: macro-file
  
  ;;; Pattern matching
  ;; This is separated out so we can use the "core" macros during the
  ;; implementation of pattern matching.
  
  (fn copy [t] (collect [k v (pairs t)] k v))
  
  (fn double-eval-safe? [x type]
    (or (= :number type) (= :string type) (= :boolean type)
        (and (sym? x) (not (multi-sym? x)))))
  
  (fn with [opts k]
    (doto (copy opts) (tset k true)))
  
  (fn without [opts k]
    (doto (copy opts) (tset k nil)))
  
  (fn case-values [vals pattern unifications case-pattern opts]
    (let [condition `(and)
          bindings []]
      (each [i pat (ipairs pattern)]
        (let [(subcondition subbindings) (case-pattern [(. vals i)] pat
                                                        unifications (without opts :multival?))]
          (table.insert condition subcondition)
          (icollect [_ b (ipairs subbindings) &into bindings] b)))
      (values condition bindings)))
  
  (fn case-table [val pattern unifications case-pattern opts]
    (let [condition `(and (= (_G.type ,val) :table))
          bindings []]
      (each [k pat (pairs pattern)]
        (if (sym? pat :&)
            (let [rest-pat (. pattern (+ k 1))
                  rest-val `(select ,k ((or table.unpack _G.unpack) ,val))
                  subcondition (case-table `(pick-values 1 ,rest-val)
                                            rest-pat unifications case-pattern
                                            (without opts :multival?))]
              (if (not (sym? rest-pat))
                  (table.insert condition subcondition))
              (assert (= nil (. pattern (+ k 2)))
                      "expected & rest argument before last parameter")
              (table.insert bindings rest-pat)
              (table.insert bindings [rest-val]))
            (sym? k :&as)
            (do
              (table.insert bindings pat)
              (table.insert bindings val))
            (and (= :number (type k)) (sym? pat :&as))
            (do
              (assert (= nil (. pattern (+ k 2)))
                      "expected &as argument before last parameter")
              (table.insert bindings (. pattern (+ k 1)))
              (table.insert bindings val))
            ;; don't process the pattern right after &/&as; already got it
            (or (not= :number (type k)) (and (not (sym? (. pattern (- k 1)) :&as))
                                             (not (sym? (. pattern (- k 1)) :&))))
            (let [subval `(. ,val ,k)
                  (subcondition subbindings) (case-pattern [subval] pat
                                                            unifications
                                                            (without opts :multival?))]
              (table.insert condition subcondition)
              (icollect [_ b (ipairs subbindings) &into bindings] b))))
      (values condition bindings)))
  
  (fn case-guard [vals condition guards unifications case-pattern opts]
    (if (. guards 1)
      (let [(pcondition bindings) (case-pattern vals condition unifications opts)
            condition `(and ,(unpack guards))]
         (values `(and ,pcondition
                       (let ,bindings
                         ,condition)) bindings))
      (case-pattern vals condition unifications opts)))
  
  (fn symbols-in-pattern [pattern]
    "gives the set of symbols inside a pattern"
    (if (list? pattern)
        (if (or (sym? (. pattern 1) :where)
                (sym? (. pattern 1) :=))
            (symbols-in-pattern (. pattern 2))
            (sym? (. pattern 2) :?)
            (symbols-in-pattern (. pattern 1))
            (let [result {}]
              (each [_ child-pattern (ipairs pattern)]
                (collect [name symbol (pairs (symbols-in-pattern child-pattern)) &into result]
                  name symbol))
              result))
        (sym? pattern)
        (if (and (not (sym? pattern :or))
                 (not (sym? pattern :nil)))
            {(tostring pattern) pattern}
            {})
        (= (type pattern) :table)
        (let [result {}]
          (each [key-pattern value-pattern (pairs pattern)]
            (collect [name symbol (pairs (symbols-in-pattern key-pattern)) &into result]
              name symbol)
            (collect [name symbol (pairs (symbols-in-pattern value-pattern)) &into result]
              name symbol))
          result)
        {}))
  
  (fn symbols-in-every-pattern [pattern-list infer-unification?]
    "gives a list of symbols that are present in every pattern in the list"
    (let [?symbols (accumulate [?symbols nil
                                _ pattern (ipairs pattern-list)]
                     (let [in-pattern (symbols-in-pattern pattern)]
                       (if ?symbols
                         (do
                           (each [name (pairs ?symbols)]
                             (when (not (. in-pattern name))
                               (tset ?symbols name nil)))
                           ?symbols)
                         in-pattern)))]
      (icollect [_ symbol (pairs (or ?symbols {}))]
        (if (not (and infer-unification?
                      (in-scope? symbol)))
          symbol))))
  
  (fn case-or [vals pattern guards unifications case-pattern opts]
    (let [pattern [(unpack pattern 2)]
          bindings (symbols-in-every-pattern pattern opts.infer-unification?)]
      (if (= nil (. bindings 1))
          ;; no bindings special case generates simple code
          (let [condition (icollect [_ subpattern (ipairs pattern) &into `(or)]
                            (case-pattern vals subpattern unifications opts))]
            (values (if (. guards 1)
                        `(and ,condition ,(unpack guards))
                        condition)
                    []))
        ;; case with bindings is handled specially, and returns three values instead of two
        (let [matched? (gensym :matched?)
              bindings-mangled (icollect [_ binding (ipairs bindings)]
                                 (gensym (tostring binding)))
              pre-bindings `(if)]
          (each [_ subpattern (ipairs pattern)]
            (let [(subcondition subbindings) (case-guard vals subpattern guards {} case-pattern opts)]
              (table.insert pre-bindings subcondition)
              (table.insert pre-bindings `(let ,subbindings
                                            (values true ,(unpack bindings))))))
          (values matched?
                  [`(,(unpack bindings)) `(values ,(unpack bindings-mangled))]
                  [`(,matched? ,(unpack bindings-mangled)) pre-bindings])))))
  
  (fn case-pattern [vals pattern unifications opts top-level?]
    "Take the AST of values and a single pattern and returns a condition
  to determine if it matches as well as a list of bindings to
  introduce for the duration of the body if it does match."
  
    ;; This function returns the following values (multival):
    ;; a "condition", which is an expression that determines whether the
    ;;   pattern should match,
    ;; a "bindings", which bind all of the symbols used in a pattern
    ;; an optional "pre-bindings", which is a list of bindings that happen
    ;;   before the condition and bindings are evaluated. These should only
    ;;   come from a (case-or). In this case there should be no recursion:
    ;;   the call stack should be case-condition > case-pattern > case-or
    ;;
    ;; Here are the expected flags in the opts table:
    ;;   :infer-unification? boolean - if the pattern should guess when to unify  (ie, match -> true, case -> false)
    ;;   :multival? boolean - if the pattern can contain multivals  (in order to disallow patterns like [(1 2)])
    ;;   :in-where? boolean - if the pattern is surrounded by (where)  (where opts into more pattern features)
    ;;   :legacy-guard-allowed? boolean - if the pattern should allow `(a ? b) patterns
  
    ;; we have to assume we're matching against multiple values here until we
    ;; know we're either in a multi-valued clause (in which case we know the #
    ;; of vals) or we're not, in which case we only care about the first one.
    (let [[val] vals]
      (if (and (sym? pattern)
               (or (sym? pattern :nil)
                   (and opts.infer-unification?
                        (in-scope? pattern)
                        (not (sym? pattern :_)))
                   (and opts.infer-unification?
                        (multi-sym? pattern)
                        (in-scope? (. (multi-sym? pattern) 1)))))
          (values `(= ,val ,pattern) [])
          ;; unify a local we've seen already
          (and (sym? pattern) (. unifications (tostring pattern)))
          (values `(= ,(. unifications (tostring pattern)) ,val) [])
          ;; bind a fresh local
          (sym? pattern)
          (let [wildcard? (: (tostring pattern) :find "^_")]
            (if (not wildcard?) (tset unifications (tostring pattern) val))
            (values (if (or wildcard? (string.find (tostring pattern) "^?")) true
                        `(not= ,(sym :nil) ,val)) [pattern val]))
          ;; opt-in unify with (=)
          (and (list? pattern)
               (sym? (. pattern 1) :=)
               (sym? (. pattern 2)))
          (let [bind (. pattern 2)]
            (assert-compile (= 2 (length pattern)) "(=) should take only one argument" pattern)
            (assert-compile (not opts.infer-unification?) "(=) cannot be used inside of match" pattern)
            (assert-compile opts.in-where? "(=) must be used in (where) patterns" pattern)
            (assert-compile (and (sym? bind) (not (sym? bind :nil)) "= has to bind to a symbol" bind))
            (values `(= ,val ,bind) []))
          ;; where-or clause
          (and (list? pattern) (sym? (. pattern 1) :where) (list? (. pattern 2)) (sym? (. pattern 2 1) :or))
          (do
            (assert-compile top-level? "can't nest (where) pattern" pattern)
            (case-or vals (. pattern 2) [(unpack pattern 3)] unifications case-pattern (with opts :in-where?)))
          ;; where clause
          (and (list? pattern) (sym? (. pattern 1) :where))
          (do
            (assert-compile top-level? "can't nest (where) pattern" pattern)
            (case-guard vals (. pattern 2) [(unpack pattern 3)] unifications case-pattern (with opts :in-where?)))
          ;; or clause (not allowed on its own)
          (and (list? pattern) (sym? (. pattern 1) :or))
          (do
            (assert-compile top-level? "can't nest (or) pattern" pattern)
            ;; This assertion can be removed to make patterns more permissive
            (assert-compile false "(or) must be used in (where) patterns" pattern)
            (case-or vals pattern [] unifications case-pattern opts))
          ;; guard clause
          (and (list? pattern) (sym? (. pattern 2) :?))
          (do
            (assert-compile opts.legacy-guard-allowed? "legacy guard clause not supported in case" pattern)
            (case-guard vals (. pattern 1) [(unpack pattern 3)] unifications case-pattern opts))
          ;; multi-valued patterns (represented as lists)
          (list? pattern)
          (do
            (assert-compile opts.multival? "can't nest multi-value destructuring" pattern)
            (case-values vals pattern unifications case-pattern opts))
          ;; table patterns
          (= (type pattern) :table)
          (case-table val pattern unifications case-pattern opts)
          ;; literal value
          (values `(= ,val ,pattern) []))))
  
  (fn add-pre-bindings [out pre-bindings]
    "Decide when to switch from the current `if` AST to a new one"
    (if pre-bindings
        ;; `out` no longer needs to grow.
        ;; Instead, a new tail `if` AST is introduced, which is where the rest of
        ;; the clauses will get appended. This way, all future clauses have the
        ;; pre-bindings in scope.
        (let [tail `(if)]
          (table.insert out true)
          (table.insert out `(let ,pre-bindings ,tail))
          tail)
        ;; otherwise, keep growing the current `if` AST.
        out))
  
  (fn case-condition [vals clauses match?]
    "Construct the actual `if` AST for the given match values and clauses."
    ;; root is the original `if` AST.
    ;; out is the `if` AST that is currently being grown.
    (let [root `(if)]
      (faccumulate [out root
                    i 1 (length clauses) 2]
        (let [pattern (. clauses i)
              body (. clauses (+ i 1))
              (condition bindings pre-bindings) (case-pattern vals pattern {}
                                                              {:multival? true
                                                               :infer-unification? match?
                                                               :legacy-guard-allowed? match?}
                                                              true)
              out (add-pre-bindings out pre-bindings)]
          ;; grow the `if` AST by one extra condition
          (table.insert out condition)
          (table.insert out `(let ,bindings
                              ,body))
          out))
      root))
  
  (fn count-case-multival [pattern]
    "Identify the amount of multival values that a pattern requires."
    (if (and (list? pattern) (sym? (. pattern 2) :?))
        (count-case-multival (. pattern 1))
        (and (list? pattern) (sym? (. pattern 1) :where))
        (count-case-multival (. pattern 2))
        (and (list? pattern) (sym? (. pattern 1) :or))
        (accumulate [longest 0
                     _ child-pattern (ipairs pattern)]
          (math.max longest (count-case-multival child-pattern)))
        (list? pattern)
        (length pattern)
        1))
  
  (fn case-count-syms [clauses]
    "Find the length of the largest multi-valued clause"
    (let [patterns (fcollect [i 1 (length clauses) 2]
                     (. clauses i))]
      (accumulate [longest 0
                   _ pattern (ipairs patterns)]
        (math.max longest (count-case-multival pattern)))))
  
  (fn maybe-optimize-table [val clauses]
    (if (faccumulate [all (sequence? val) i 1 (length clauses) 2 &until (not all)]
          (and (sequence? (. clauses i))
               (accumulate [all2 (next (. clauses i))
                            _ d (ipairs (. clauses i)) &until (not all2)]
                 (and all2 (or (not (sym? d)) (not (: (tostring d) :find "^&")))))))
        (values `(values ,(unpack val))
                (fcollect [i 1 (length clauses)]
                  (if (= 1 (% i 2))
                      (list (unpack (. clauses i)))
                      (. clauses i))))
        (values val clauses)))
  
  (fn case-impl [match? val ...]
    "The shared implementation of case and match."
    (assert (not= val nil) "missing subject")
    (assert (= 0 (math.fmod (select :# ...) 2))
            "expected even number of pattern/body pairs")
    (assert (not= 0 (select :# ...))
            "expected at least one pattern/body pair")
    (let [(val clauses) (maybe-optimize-table val [...])
          vals-count (case-count-syms clauses)
          skips-multiple-eval-protection? (and (= vals-count 1) (double-eval-safe? val))]
      (if skips-multiple-eval-protection?
        (case-condition (list val) clauses match?)
        ;; protect against multiple evaluation of the value, bind against as
        ;; many values as we ever match against in the clauses.
        (let [vals (fcollect [_ 1 vals-count &into (list)] (gensym))]
          (list `let [vals val] (case-condition vals clauses match?))))))
  
  (fn case* [val ...]
    "Perform pattern matching on val. See reference for details.
  
  Syntax:
  
  (case data-expression
    pattern body
    (where pattern guards*) body
    (where (or pattern patterns*) guards*) body)"
    (case-impl false val ...))
  
  (fn match* [val ...]
    "Perform pattern matching on val, automatically unifying on variables in
  local scope. See reference for details.
  
  Syntax:
  
  (match data-expression
    pattern body
    (where pattern guards*) body
    (where (or pattern patterns*) guards*) body)"
    (case-impl true val ...))
  
  (fn case-try-step [how expr else pattern body ...]
    (if (= nil pattern body)
        expr
        ;; unlike regular match, we can't know how many values the value
        ;; might evaluate to, so we have to capture them all in ... via IIFE
        ;; to avoid double-evaluation.
        `((fn [...]
            (,how ...
              ,pattern ,(case-try-step how body else ...)
              ,(unpack else)))
          ,expr)))
  
  (fn case-try-impl [how expr pattern body ...]
    (let [clauses [pattern body ...]
          last (. clauses (length clauses))
          catch (if (sym? (and (= :table (type last)) (. last 1)) :catch)
                   (let [[_ & e] (table.remove clauses)] e) ; remove `catch sym
                   [`_# `...])]
      (assert (= 0 (math.fmod (length clauses) 2))
              "expected every pattern to have a body")
      (assert (= 0 (math.fmod (length catch) 2))
              "expected every catch pattern to have a body")
      (case-try-step how expr catch (unpack clauses))))
  
  (fn case-try* [expr pattern body ...]
    "Perform chained pattern matching for a sequence of steps which might fail.
  
  The values from the initial expression are matched against the first pattern.
  If they match, the first body is evaluated and its values are matched against
  the second pattern, etc.
  
  If there is a (catch pat1 body1 pat2 body2 ...) form at the end, any mismatch
  from the steps will be tried against these patterns in sequence as a fallback
  just like a normal match. If there is no catch, the mismatched values will be
  returned as the value of the entire expression."
    (case-try-impl `case expr pattern body ...))
  
  (fn match-try* [expr pattern body ...]
    "Perform chained pattern matching for a sequence of steps which might fail.
  
  The values from the initial expression are matched against the first pattern.
  If they match, the first body is evaluated and its values are matched against
  the second pattern, etc.
  
  If there is a (catch pat1 body1 pat2 body2 ...) form at the end, any mismatch
  from the steps will be tried against these patterns in sequence as a fallback
  just like a normal match. If there is no catch, the mismatched values will be
  returned as the value of the entire expression."
    (case-try-impl `match expr pattern body ...))
  
  {:case case*
   :case-try case-try*
   :match match*
   :match-try match-try*}
  ]===], {allowedGlobals = false, env = env, filename = "src/fennel/match.fnl", moduleName = module_name, scope = compiler.scopes.compiler, useMetadata = true})
  for k, v in pairs(match_macros) do
    compiler.scopes.global.macros[k] = v
  end
  package.preload[module_name] = nil
end
return mod
