package.preload["fennel.repl"] = package.preload["fennel.repl"] or function(...)
  local utils = require("fennel.utils")
  local parser = require("fennel.parser")
  local compiler = require("fennel.compiler")
  local specials = require("fennel.specials")
  local view = require("fennel.view")
  local unpack = (table.unpack or _G.unpack)
  local function default_read_chunk(parser_state)
    local function _519_()
      if (0 < parser_state["stack-size"]) then
        return ".."
      else
        return ">> "
      end
    end
    io.write(_519_())
    io.flush()
    local input = io.read()
    return (input and (input .. "\n"))
  end
  local function default_on_values(xs)
    io.write(table.concat(xs, "\9"))
    return io.write("\n")
  end
  local function default_on_error(errtype, err, lua_source)
    local function _521_()
      local _520_ = errtype
      if (_520_ == "Lua Compile") then
        return ("Bad code generated - likely a bug with the compiler:\n" .. "--- Generated Lua Start ---\n" .. lua_source .. "--- Generated Lua End ---\n")
      elseif (_520_ == "Runtime") then
        return (compiler.traceback(tostring(err), 4) .. "\n")
      elseif true then
        local _ = _520_
        return ("%s error: %s\n"):format(errtype, tostring(err))
      else
        return nil
      end
    end
    return io.write(_521_())
  end
  local save_source = table.concat({"local ___i___ = 1", "while true do", " local name, value = debug.getlocal(1, ___i___)", " if(name and name ~= \"___i___\") then", " ___replLocals___[name] = value", " ___i___ = ___i___ + 1", " else break end end"}, "\n")
  local function splice_save_locals(env, lua_source)
    local spliced_source = {}
    local bind = "local %s = ___replLocals___['%s']"
    for line in lua_source:gmatch("([^\n]+)\n?") do
      table.insert(spliced_source, line)
    end
    for name in pairs(env.___replLocals___) do
      table.insert(spliced_source, 1, bind:format(name, name))
    end
    if ((1 < #spliced_source) and (spliced_source[#spliced_source]):match("^ *return .*$")) then
      table.insert(spliced_source, #spliced_source, save_source)
    else
    end
    return table.concat(spliced_source, "\n")
  end
  local function completer(env, scope, text)
    local matches = {}
    local input_fragment = text:gsub(".*[%s)(]+", "")
    local stop_looking_3f = false
    local function add_partials(input, tbl, prefix, method_3f)
      for k in utils.allpairs(tbl) do
        local k0
        if ((tbl == env) or (tbl == env.___replLocals___)) then
          k0 = scope.unmanglings[k]
        else
          k0 = k
        end
        if ((#matches < 2000) and (type(k0) == "string") and (input == k0:sub(0, #input)) and (not method_3f or ("function" == type(tbl[k0])))) then
          local function _525_()
            if method_3f then
              return (prefix .. ":" .. k0)
            else
              return (prefix .. k0)
            end
          end
          table.insert(matches, _525_())
        else
        end
      end
      return nil
    end
    local function descend(input, tbl, prefix, add_matches, method_3f)
      local splitter
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
          return add_partials(tail, tbl[raw_head], (prefix .. head), true)
        else
          return add_matches(tail, tbl[raw_head], (prefix .. head))
        end
      else
        return nil
      end
    end
    local function add_matches(input, tbl, prefix)
      local prefix0
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
    for _, source in ipairs({scope.specials, scope.macros, (env.___replLocals___ or {}), env, env._G}) do
      if stop_looking_3f then break end
      add_matches(input_fragment, source)
    end
    return matches
  end
  local commands = {}
  local function command_3f(input)
    return input:match("^%s*,")
  end
  local function command_docs()
    local _532_
    do
      local tbl_14_auto = {}
      local i_15_auto = #tbl_14_auto
      for name, f in pairs(commands) do
        local val_16_auto = ("  ,%s - %s"):format(name, ((compiler.metadata):get(f, "fnl/docstring") or "undocumented"))
        if (nil ~= val_16_auto) then
          i_15_auto = (i_15_auto + 1)
          do end (tbl_14_auto)[i_15_auto] = val_16_auto
        else
        end
      end
      _532_ = tbl_14_auto
    end
    return table.concat(_532_, "\n")
  end
  commands.help = function(_, _0, on_values)
    return on_values({("Welcome to Fennel.\nThis is the REPL where you can enter code to be evaluated.\nYou can also run these repl commands:\n\n" .. command_docs() .. "\n  ,exit - Leave the repl.\n\nUse ,doc something to see descriptions for individual macros and special forms.\n\nFor more information about the language, see https://fennel-lang.org/reference")})
  end
  do end (compiler.metadata):set(commands.help, "fnl/docstring", "Show this message.")
  local function reload(module_name, env, on_values, on_error)
    local _534_, _535_ = pcall(specials["load-code"]("return require(...)", env), module_name)
    if ((_534_ == true) and (nil ~= _535_)) then
      local old = _535_
      local _
      package.loaded[module_name] = nil
      _ = nil
      local ok, new = pcall(require, module_name)
      local new0
      if not ok then
        on_values({new})
        new0 = old
      else
        new0 = new
      end
      if ((type(old) == "table") and (type(new0) == "table")) then
        for k, v in pairs(new0) do
          old[k] = v
        end
        for k in pairs(old) do
          if (nil == (new0)[k]) then
            old[k] = nil
          else
          end
        end
        package.loaded[module_name] = old
      else
      end
      return on_values({"ok"})
    elseif ((_534_ == false) and (nil ~= _535_)) then
      local msg = _535_
      local function _540_()
        local _539_ = msg:gsub("\n.*", "")
        return _539_
      end
      return on_error("Runtime", _540_())
    else
      return nil
    end
  end
  local function run_command(read, on_error, f)
    local _542_, _543_, _544_ = pcall(read)
    if ((_542_ == true) and (_543_ == true) and (nil ~= _544_)) then
      local val = _544_
      return f(val)
    elseif (_542_ == false) then
      return on_error("Parse", "Couldn't parse input.")
    else
      return nil
    end
  end
  commands.reload = function(env, read, on_values, on_error)
    local function _546_(_241)
      return reload(tostring(_241), env, on_values, on_error)
    end
    return run_command(read, on_error, _546_)
  end
  do end (compiler.metadata):set(commands.reload, "fnl/docstring", "Reload the specified module.")
  commands.reset = function(env, _, on_values)
    env.___replLocals___ = {}
    return on_values({"ok"})
  end
  do end (compiler.metadata):set(commands.reset, "fnl/docstring", "Erase all repl-local scope.")
  commands.complete = function(env, read, on_values, on_error, scope, chars)
    local function _547_()
      return on_values(completer(env, scope, string.char(unpack(chars)):gsub(",complete +", ""):sub(1, -2)))
    end
    return run_command(read, on_error, _547_)
  end
  do end (compiler.metadata):set(commands.complete, "fnl/docstring", "Print all possible completions for a given input symbol.")
  local function apropos_2a(pattern, tbl, prefix, seen, names)
    for name, subtbl in pairs(tbl) do
      if (("string" == type(name)) and (package ~= subtbl)) then
        local _548_ = type(subtbl)
        if (_548_ == "function") then
          if ((prefix .. name)):match(pattern) then
            table.insert(names, (prefix .. name))
          else
          end
        elseif (_548_ == "table") then
          if not seen[subtbl] then
            local _551_
            do
              local _550_ = seen
              _550_[subtbl] = true
              _551_ = _550_
            end
            apropos_2a(pattern, subtbl, (prefix .. name:gsub("%.", "/") .. "."), _551_, names)
          else
          end
        else
        end
      else
      end
    end
    return names
  end
  local function apropos(pattern)
    local names = apropos_2a(pattern, package.loaded, "", {}, {})
    local tbl_14_auto = {}
    local i_15_auto = #tbl_14_auto
    for _, name in ipairs(names) do
      local val_16_auto = name:gsub("^_G%.", "")
      if (nil ~= val_16_auto) then
        i_15_auto = (i_15_auto + 1)
        do end (tbl_14_auto)[i_15_auto] = val_16_auto
      else
      end
    end
    return tbl_14_auto
  end
  commands.apropos = function(_env, read, on_values, on_error, _scope)
    local function _556_(_241)
      return on_values(apropos(tostring(_241)))
    end
    return run_command(read, on_error, _556_)
  end
  do end (compiler.metadata):set(commands.apropos, "fnl/docstring", "Print all functions matching a pattern in all loaded modules.")
  local function apropos_follow_path(path)
    local paths
    do
      local tbl_14_auto = {}
      local i_15_auto = #tbl_14_auto
      for p in path:gmatch("[^%.]+") do
        local val_16_auto = p
        if (nil ~= val_16_auto) then
          i_15_auto = (i_15_auto + 1)
          do end (tbl_14_auto)[i_15_auto] = val_16_auto
        else
        end
      end
      paths = tbl_14_auto
    end
    local tgt = package.loaded
    for _, path0 in ipairs(paths) do
      if (nil == tgt) then break end
      local _559_
      do
        local _558_ = path0:gsub("%/", ".")
        _559_ = _558_
      end
      tgt = tgt[_559_]
    end
    return tgt
  end
  local function apropos_doc(pattern)
    local names = {}
    for _, path in ipairs(apropos(".*")) do
      local tgt = apropos_follow_path(path)
      if ("function" == type(tgt)) then
        local _560_ = (compiler.metadata):get(tgt, "fnl/docstring")
        if (nil ~= _560_) then
          local docstr = _560_
          if docstr:match(pattern) then
            table.insert(names, path)
          else
          end
        else
        end
      else
      end
    end
    return names
  end
  commands["apropos-doc"] = function(_env, read, on_values, on_error, _scope)
    local function _564_(_241)
      return on_values(apropos_doc(tostring(_241)))
    end
    return run_command(read, on_error, _564_)
  end
  do end (compiler.metadata):set(commands["apropos-doc"], "fnl/docstring", "Print all functions that match the pattern in their docs")
  local function apropos_show_docs(on_values, pattern)
    for _, path in ipairs(apropos(pattern)) do
      local tgt = apropos_follow_path(path)
      if (("function" == type(tgt)) and (compiler.metadata):get(tgt, "fnl/docstring")) then
        on_values(specials.doc(tgt, path))
        on_values()
      else
      end
    end
    return nil
  end
  commands["apropos-show-docs"] = function(_env, read, on_values, on_error)
    local function _566_(_241)
      return apropos_show_docs(on_values, tostring(_241))
    end
    return run_command(read, on_error, _566_)
  end
  do end (compiler.metadata):set(commands["apropos-show-docs"], "fnl/docstring", "Print all documentations matching a pattern in function name")
  local function resolve(identifier, _567_, scope)
    local _arg_568_ = _567_
    local ___replLocals___ = _arg_568_["___replLocals___"]
    local env = _arg_568_
    local e
    local function _569_(_241, _242)
      return (___replLocals___[_242] or env[_242])
    end
    e = setmetatable({}, {__index = _569_})
    local code = compiler["compile-string"](tostring(identifier), {scope = scope})
    return specials["load-code"](code, e)()
  end
  commands.find = function(env, read, on_values, on_error, scope)
    local function _570_(_241)
      local _571_
      do
        local _572_ = utils["sym?"](_241)
        if (nil ~= _572_) then
          local _573_ = resolve(_572_, env, scope)
          if (nil ~= _573_) then
            _571_ = debug.getinfo(_573_)
          else
            _571_ = _573_
          end
        else
          _571_ = _572_
        end
      end
      if ((_G.type(_571_) == "table") and (nil ~= (_571_).short_src) and (nil ~= (_571_).linedefined) and (nil ~= (_571_).source) and ((_571_).what == "Lua")) then
        local src = (_571_).short_src
        local line = (_571_).linedefined
        local source = (_571_).source
        local fnlsrc
        do
          local t_576_ = compiler.sourcemap
          if (nil ~= t_576_) then
            t_576_ = (t_576_)[source]
          else
          end
          if (nil ~= t_576_) then
            t_576_ = (t_576_)[line]
          else
          end
          if (nil ~= t_576_) then
            t_576_ = (t_576_)[2]
          else
          end
          fnlsrc = t_576_
        end
        return on_values({string.format("%s:%s", src, (fnlsrc or line))})
      elseif (_571_ == nil) then
        return on_error("Repl", "Unknown value")
      elseif true then
        local _ = _571_
        return on_error("Repl", "No source info")
      else
        return nil
      end
    end
    return run_command(read, on_error, _570_)
  end
  do end (compiler.metadata):set(commands.find, "fnl/docstring", "Print the filename and line number for a given function")
  commands.doc = function(env, read, on_values, on_error, scope)
    local function _581_(_241)
      local name = tostring(_241)
      local target = (scope.specials[name] or scope.macros[name] or resolve(name, env, scope))
      return on_values({specials.doc(target, name)})
    end
    return run_command(read, on_error, _581_)
  end
  do end (compiler.metadata):set(commands.doc, "fnl/docstring", "Print the docstring and arglist for a function, macro, or special form.")
  local function load_plugin_commands(plugins)
    for _, plugin in ipairs((plugins or {})) do
      for name, f in pairs(plugin) do
        local _582_ = name:match("^repl%-command%-(.*)")
        if (nil ~= _582_) then
          local cmd_name = _582_
          commands[cmd_name] = (commands[cmd_name] or f)
        else
        end
      end
    end
    return nil
  end
  local function run_command_loop(input, read, loop, env, on_values, on_error, scope, chars)
    local command_name = input:match(",([^%s/]+)")
    do
      local _584_ = commands[command_name]
      if (nil ~= _584_) then
        local command = _584_
        command(env, read, on_values, on_error, scope, chars)
      elseif true then
        local _ = _584_
        if ("exit" ~= command_name) then
          on_values({"Unknown command", command_name})
        else
        end
      else
      end
    end
    if ("exit" ~= command_name) then
      return loop()
    else
      return nil
    end
  end
  local function repl(options)
    local old_root_options = utils.root.options
    local env = specials["wrap-env"]((options.env or (rawget(_G, "_ENV") or _G)))
    local save_locals_3f = ((options.saveLocals ~= false) and env.debug and env.debug.getlocal)
    local opts = utils.copy(options)
    local read_chunk = (opts.readChunk or default_read_chunk)
    local on_values = (opts.onValues or default_on_values)
    local on_error = (opts.onError or default_on_error)
    local pp = (opts.pp or view)
    local byte_stream, clear_stream = parser.granulate(read_chunk)
    local chars = {}
    local read, reset = nil, nil
    local function _588_(parser_state)
      local c = byte_stream(parser_state)
      table.insert(chars, c)
      return c
    end
    read, reset = parser.parser(_588_)
    opts.env, opts.scope = env, compiler["make-scope"]()
    opts.useMetadata = (options.useMetadata ~= false)
    if (opts.allowedGlobals == nil) then
      opts.allowedGlobals = specials["current-global-names"](env)
    else
    end
    if opts.registerCompleter then
      local function _592_()
        local _590_ = env
        local _591_ = opts.scope
        local function _593_(...)
          return completer(_590_, _591_, ...)
        end
        return _593_
      end
      opts.registerCompleter(_592_())
    else
    end
    load_plugin_commands(opts.plugins)
    if save_locals_3f then
      local function newindex(t, k, v)
        if opts.scope.unmanglings[k] then
          return rawset(t, k, v)
        else
          return nil
        end
      end
      env.___replLocals___ = setmetatable({}, {__newindex = newindex})
    else
    end
    local function print_values(...)
      local vals = {...}
      local out = {}
      env._, env.__ = vals[1], vals
      for i = 1, select("#", ...) do
        table.insert(out, pp(vals[i]))
      end
      return on_values(out)
    end
    local function loop()
      for k in pairs(chars) do
        chars[k] = nil
      end
      reset()
      local ok, parse_ok_3f, x = pcall(read)
      local src_string = string.char(unpack(chars))
      if not ok then
        on_error("Parse", parse_ok_3f)
        clear_stream()
        return loop()
      elseif command_3f(src_string) then
        return run_command_loop(src_string, read, loop, env, on_values, on_error, opts.scope, chars)
      else
        if parse_ok_3f then
          do
            local _597_, _598_ = nil, nil
            local function _600_()
              local _599_ = opts
              _599_["source"] = src_string
              return _599_
            end
            _597_, _598_ = pcall(compiler.compile, x, _600_())
            if ((_597_ == false) and (nil ~= _598_)) then
              local msg = _598_
              clear_stream()
              on_error("Compile", msg)
            elseif ((_597_ == true) and (nil ~= _598_)) then
              local src = _598_
              local src0
              if save_locals_3f then
                src0 = splice_save_locals(env, src, opts.scope)
              else
                src0 = src
              end
              local _602_, _603_ = pcall(specials["load-code"], src0, env)
              if ((_602_ == false) and (nil ~= _603_)) then
                local msg = _603_
                clear_stream()
                on_error("Lua Compile", msg, src0)
              elseif (true and (nil ~= _603_)) then
                local _ = _602_
                local chunk = _603_
                local function _604_()
                  return print_values(chunk())
                end
                local function _605_()
                  local function _606_(...)
                    return on_error("Runtime", ...)
                  end
                  return _606_
                end
                xpcall(_604_, _605_())
              else
              end
            else
            end
          end
          utils.root.options = old_root_options
          return loop()
        else
          return nil
        end
      end
    end
    return loop()
  end
  return repl
end
package.preload["fennel.specials"] = package.preload["fennel.specials"] or function(...)
  local utils = require("fennel.utils")
  local view = require("fennel.view")
  local parser = require("fennel.parser")
  local compiler = require("fennel.compiler")
  local unpack = (table.unpack or _G.unpack)
  local SPECIALS = compiler.scopes.global.specials
  local function wrap_env(env)
    local function _345_(_, key)
      if (type(key) == "string") then
        return env[compiler["global-unmangling"](key)]
      else
        return env[key]
      end
    end
    local function _347_(_, key, value)
      if (type(key) == "string") then
        env[compiler["global-unmangling"](key)] = value
        return nil
      else
        env[key] = value
        return nil
      end
    end
    local function _349_()
      local function putenv(k, v)
        local _350_
        if (type(k) == "string") then
          _350_ = compiler["global-unmangling"](k)
        else
          _350_ = k
        end
        return _350_, v
      end
      return next, utils.kvmap(env, putenv), nil
    end
    return setmetatable({}, {__index = _345_, __newindex = _347_, __pairs = _349_})
  end
  local function current_global_names(_3fenv)
    local mt
    do
      local _352_ = getmetatable(_3fenv)
      if ((_G.type(_352_) == "table") and (nil ~= (_352_).__pairs)) then
        local mtpairs = (_352_).__pairs
        local tbl_11_auto = {}
        for k, v in mtpairs(_3fenv) do
          local _353_, _354_ = k, v
          if ((nil ~= _353_) and (nil ~= _354_)) then
            local k_12_auto = _353_
            local v_13_auto = _354_
            tbl_11_auto[k_12_auto] = v_13_auto
          else
          end
        end
        mt = tbl_11_auto
      elseif (_352_ == nil) then
        mt = (_3fenv or _G)
      else
        mt = nil
      end
    end
    return (mt and utils.kvmap(mt, compiler["global-unmangling"]))
  end
  local function load_code(code, _3fenv, _3ffilename)
    local env = (_3fenv or rawget(_G, "_ENV") or _G)
    if (rawget(_G, "setfenv") and rawget(_G, "loadstring")) then
      local f = assert(_G.loadstring(code, _3ffilename))
      local _357_ = f
      setfenv(_357_, env)
      return _357_
    else
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
        local arglist = table.concat(((compiler.metadata):get(tgt, "fnl/arglist") or {"#<unknown-arguments>"}), " ")
        local _359_
        if (#arglist > 0) then
          _359_ = " "
        else
          _359_ = ""
        end
        return string.format("(%s%s%s)\n  %s", name, _359_, arglist, docstring)
      else
        return string.format("%s\n  %s", name, docstring)
      end
    end
  end
  local function doc_special(name, arglist, docstring, body_form_3f)
    compiler.metadata[SPECIALS[name]] = {["fnl/arglist"] = arglist, ["fnl/docstring"] = docstring, ["fnl/body-form?"] = body_form_3f}
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
    local function compile_body(outer_target, outer_tail, outer_retexprs)
      if (len < start) then
        compiler.compile1(nil, sub_scope, chunk, {tail = outer_tail, target = outer_target})
      else
        for i = start, len do
          local subopts = {nval = (((i ~= len) and 0) or opts.nval), tail = (((i == len) and outer_tail) or nil), target = (((i == len) and outer_target) or nil)}
          local _ = utils["propagate-options"](opts, subopts)
          local subexprs = compiler.compile1(ast[i], sub_scope, chunk, subopts)
          if (i ~= len) then
            compiler["keep-side-effects"](subexprs, parent, nil, ast[i])
          else
          end
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
        local s = ((_3fpre_syms and (_3fpre_syms)[i]) or compiler.gensym(scope))
        do end (syms)[i] = s
        retexprs[i] = utils.expr(s, "sym")
      end
      local outer_target = table.concat(syms, ", ")
      compiler.emit(parent, string.format("local %s", outer_target), ast)
      compiler.emit(parent, "do", ast)
      return compile_body(outer_target, opts.tail)
    else
      local fname = compiler.gensym(scope)
      local fargs
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
  SPECIALS.values = function(ast, scope, parent)
    local len = #ast
    local exprs = {}
    for i = 2, len do
      local subexprs = compiler.compile1(ast[i], scope, parent, {nval = ((i ~= len) and 1)})
      table.insert(exprs, subexprs[1])
      if (i == len) then
        for j = 2, #subexprs do
          table.insert(exprs, subexprs[j])
        end
      else
      end
    end
    return exprs
  end
  doc_special("values", {"..."}, "Return multiple values from a function. Must be in tail position.")
  local function deep_tostring(x, key_3f)
    if utils["sequence?"](x) then
      local _368_
      do
        local tbl_14_auto = {}
        local i_15_auto = #tbl_14_auto
        for _, v in ipairs(x) do
          local val_16_auto = deep_tostring(v)
          if (nil ~= val_16_auto) then
            i_15_auto = (i_15_auto + 1)
            do end (tbl_14_auto)[i_15_auto] = val_16_auto
          else
          end
        end
        _368_ = tbl_14_auto
      end
      return ("[" .. table.concat(_368_, " ") .. "]")
    elseif utils["table?"](x) then
      local _370_
      do
        local tbl_14_auto = {}
        local i_15_auto = #tbl_14_auto
        for k, v in pairs(x) do
          local val_16_auto = (deep_tostring(k, true) .. " " .. deep_tostring(v))
          if (nil ~= val_16_auto) then
            i_15_auto = (i_15_auto + 1)
            do end (tbl_14_auto)[i_15_auto] = val_16_auto
          else
          end
        end
        _370_ = tbl_14_auto
      end
      return ("{" .. table.concat(_370_, " ") .. "}")
    elseif (key_3f and (type(x) == "string") and x:find("^[-%w?\\^_!$%&*+./@:|<=>]+$")) then
      return (":" .. x)
    elseif (type(x) == "string") then
      return string.format("%q", x):gsub("\\\"", "\\\\\""):gsub("\"", "\\\"")
    else
      return tostring(x)
    end
  end
  local function set_fn_metadata(arg_list, docstring, parent, fn_name)
    if utils.root.options.useMetadata then
      local args
      local function _373_(_241)
        return ("\"%s\""):format(deep_tostring(_241))
      end
      args = utils.map(arg_list, _373_)
      local meta_fields = {"\"fnl/arglist\"", ("{" .. table.concat(args, ", ") .. "}")}
      if docstring then
        table.insert(meta_fields, "\"fnl/docstring\"")
        table.insert(meta_fields, ("\"" .. docstring:gsub("%s+$", ""):gsub("\\", "\\\\"):gsub("\n", "\\n"):gsub("\"", "\\\"") .. "\""))
      else
      end
      local meta_str = ("require(\"%s\").metadata"):format((utils.root.options.moduleName or "fennel"))
      return compiler.emit(parent, ("pcall(function() %s:setall(%s, %s) end)"):format(meta_str, fn_name, table.concat(meta_fields, ", ")))
    else
      return nil
    end
  end
  local function get_fn_name(ast, scope, fn_name, multi)
    if (fn_name and (fn_name[1] ~= "nil")) then
      local _376_
      if not multi then
        _376_ = compiler["declare-local"](fn_name, {}, scope, ast)
      else
        _376_ = (compiler["symbol-to-expression"](fn_name, scope))[1]
      end
      return _376_, not multi, 3
    else
      return nil, true, 2
    end
  end
  local function compile_named_fn(ast, f_scope, f_chunk, parent, index, fn_name, local_3f, arg_name_list, arg_list, docstring)
    for i = (index + 1), #ast do
      compiler.compile1(ast[i], f_scope, f_chunk, {nval = (((i ~= #ast) and 0) or nil), tail = (i == #ast)})
    end
    local _379_
    if local_3f then
      _379_ = "local function %s(%s)"
    else
      _379_ = "%s = function(%s)"
    end
    compiler.emit(parent, string.format(_379_, fn_name, table.concat(arg_name_list, ", ")), ast)
    compiler.emit(parent, f_chunk, ast)
    compiler.emit(parent, "end", ast)
    set_fn_metadata(arg_list, docstring, parent, fn_name)
    utils.hook("fn", ast, f_scope)
    return utils.expr(fn_name, "sym")
  end
  local function compile_anonymous_fn(ast, f_scope, f_chunk, parent, index, arg_name_list, arg_list, docstring, scope)
    local fn_name = compiler.gensym(scope)
    return compile_named_fn(ast, f_scope, f_chunk, parent, index, fn_name, true, arg_name_list, arg_list, docstring)
  end
  SPECIALS.fn = function(ast, scope, parent)
    local f_scope
    do
      local _381_ = compiler["make-scope"](scope)
      do end (_381_)["vararg"] = false
      f_scope = _381_
    end
    local f_chunk = {}
    local fn_sym = utils["sym?"](ast[2])
    local multi = (fn_sym and utils["multi-sym?"](fn_sym[1]))
    local fn_name, local_3f, index = get_fn_name(ast, scope, fn_sym, multi)
    local arg_list = compiler.assert(utils["table?"](ast[index]), "expected parameters table", ast)
    compiler.assert((not multi or not multi["multi-sym-method-call"]), ("unexpected multi symbol " .. tostring(fn_name)), fn_sym)
    local function get_arg_name(arg)
      if utils["varg?"](arg) then
        compiler.assert((arg == arg_list[#arg_list]), "expected vararg as last parameter", ast)
        f_scope.vararg = true
        return "..."
      elseif (utils["sym?"](arg) and (tostring(arg) ~= "nil") and not utils["multi-sym?"](tostring(arg))) then
        return compiler["declare-local"](arg, {}, f_scope, ast)
      elseif utils["table?"](arg) then
        local raw = utils.sym(compiler.gensym(scope))
        local declared = compiler["declare-local"](raw, {}, f_scope, ast)
        compiler.destructure(arg, raw, ast, f_scope, f_chunk, {declaration = true, nomulti = true, symtype = "arg"})
        return declared
      else
        return compiler.assert(false, ("expected symbol for function parameter: %s"):format(tostring(arg)), ast[index])
      end
    end
    local arg_name_list = utils.map(arg_list, get_arg_name)
    local index0, docstring = nil, nil
    if ((type(ast[(index + 1)]) == "string") and ((index + 1) < #ast)) then
      index0, docstring = (index + 1), ast[(index + 1)]
    else
      index0, docstring = index, nil
    end
    if fn_name then
      return compile_named_fn(ast, f_scope, f_chunk, parent, index0, fn_name, local_3f, arg_name_list, arg_list, docstring)
    else
      return compile_anonymous_fn(ast, f_scope, f_chunk, parent, index0, arg_name_list, arg_list, docstring, scope)
    end
  end
  doc_special("fn", {"name?", "args", "docstring?", "..."}, "Function syntax. May optionally include a name and docstring.\nIf a name is provided, the function will be bound in the current scope.\nWhen called with the wrong number of args, excess args will be discarded\nand lacking args will be nil, use lambda for arity-checked functions.", true)
  SPECIALS.lua = function(ast, _, parent)
    compiler.assert(((#ast == 2) or (#ast == 3)), "expected 1 or 2 arguments", ast)
    local _386_
    do
      local _385_ = utils["sym?"](ast[2])
      if (nil ~= _385_) then
        _386_ = tostring(_385_)
      else
        _386_ = _385_
      end
    end
    if ("nil" ~= _386_) then
      table.insert(parent, {ast = ast, leaf = tostring(ast[2])})
    else
    end
    local _390_
    do
      local _389_ = utils["sym?"](ast[3])
      if (nil ~= _389_) then
        _390_ = tostring(_389_)
      else
        _390_ = _389_
      end
    end
    if ("nil" ~= _390_) then
      return tostring(ast[3])
    else
      return nil
    end
  end
  local function dot(ast, scope, parent)
    compiler.assert((1 < #ast), "expected table argument", ast)
    local len = #ast
    local _let_393_ = compiler.compile1(ast[2], scope, parent, {nval = 1})
    local lhs = _let_393_[1]
    if (len == 2) then
      return tostring(lhs)
    else
      local indices = {}
      for i = 3, len do
        local index = ast[i]
        if ((type(index) == "string") and utils["valid-lua-identifier?"](index)) then
          table.insert(indices, ("." .. index))
        else
          local _let_394_ = compiler.compile1(index, scope, parent, {nval = 1})
          local index0 = _let_394_[1]
          table.insert(indices, ("[" .. tostring(index0) .. "]"))
        end
      end
      if (tostring(lhs):find("[{\"0-9]") or ("nil" == tostring(lhs))) then
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
  doc_special("global", {"name", "val"}, "Set name as a global with val.")
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
  local function local_2a(ast, scope, parent)
    compiler.assert((#ast == 3), "expected name and value", ast)
    compiler.destructure(ast[2], ast[3], ast, scope, parent, {declaration = true, nomulti = true, symtype = "local"})
    return nil
  end
  SPECIALS["local"] = local_2a
  doc_special("local", {"name", "val"}, "Introduce new top-level immutable local.")
  SPECIALS.var = function(ast, scope, parent)
    compiler.assert((#ast == 3), "expected name and value", ast)
    compiler.destructure(ast[2], ast[3], ast, scope, parent, {declaration = true, isvar = true, nomulti = true, symtype = "var"})
    return nil
  end
  doc_special("var", {"name", "val"}, "Introduce new mutable local.")
  local function kv_3f(t)
    local _398_
    do
      local tbl_14_auto = {}
      local i_15_auto = #tbl_14_auto
      for k in pairs(t) do
        local val_16_auto
        if not ("number" == type(k)) then
          val_16_auto = k
        else
          val_16_auto = nil
        end
        if (nil ~= val_16_auto) then
          i_15_auto = (i_15_auto + 1)
          do end (tbl_14_auto)[i_15_auto] = val_16_auto
        else
        end
      end
      _398_ = tbl_14_auto
    end
    return (_398_)[1]
  end
  SPECIALS.let = function(ast, scope, parent, opts)
    local bindings = ast[2]
    local pre_syms = {}
    compiler.assert((utils["table?"](bindings) and not kv_3f(bindings)), "expected binding sequence", bindings)
    compiler.assert(((#bindings % 2) == 0), "expected even number of name/value bindings", ast[2])
    compiler.assert((#ast >= 3), "expected body expression", ast[1])
    for _ = 1, (opts.nval or 0) do
      table.insert(pre_syms, compiler.gensym(scope))
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
  local function disambiguate_3f(rootstr, parent)
    local function _403_()
      local _402_ = get_prev_line(parent)
      if (nil ~= _402_) then
        local prev_line = _402_
        return prev_line:match("%)$")
      else
        return nil
      end
    end
    return (rootstr:match("^{") or _403_())
  end
  SPECIALS.tset = function(ast, scope, parent)
    compiler.assert((#ast > 3), "expected table, key, and value arguments", ast)
    local root = (compiler.compile1(ast[2], scope, parent, {nval = 1}))[1]
    local keys = {}
    for i = 3, (#ast - 1) do
      local _let_405_ = compiler.compile1(ast[i], scope, parent, {nval = 1})
      local key = _let_405_[1]
      table.insert(keys, tostring(key))
    end
    local value = (compiler.compile1(ast[#ast], scope, parent, {nval = 1}))[1]
    local rootstr = tostring(root)
    local fmtstr
    if disambiguate_3f(rootstr, parent) then
      fmtstr = "do end (%s)[%s] = %s"
    else
      fmtstr = "%s[%s] = %s"
    end
    return compiler.emit(parent, fmtstr:format(rootstr, table.concat(keys, "]["), tostring(value)), ast)
  end
  doc_special("tset", {"tbl", "key1", "...", "keyN", "val"}, "Set the value of a table field. Can take additional keys to set\nnested values, but all parents must contain an existing table.")
  local function calculate_target(scope, opts)
    if not (opts.tail or opts.target or opts.nval) then
      return "iife", true, nil
    elseif (opts.nval and (opts.nval ~= 0) and not opts.target) then
      local accum = {}
      local target_exprs = {}
      for i = 1, opts.nval do
        local s = compiler.gensym(scope)
        do end (accum)[i] = s
        target_exprs[i] = utils.expr(s, "sym")
      end
      return "target", opts.tail, table.concat(accum, ", "), target_exprs
    else
      return "none", opts.tail, opts.target
    end
  end
  local function if_2a(ast, scope, parent, opts)
    compiler.assert((2 < #ast), "expected condition and body", ast)
    local do_scope = compiler["make-scope"](scope)
    local branches = {}
    local wrapper, inner_tail, inner_target, target_exprs = calculate_target(scope, opts)
    local body_opts = {nval = opts.nval, tail = inner_tail, target = inner_target}
    local function compile_body(i)
      local chunk = {}
      local cscope = compiler["make-scope"](do_scope)
      compiler["keep-side-effects"](compiler.compile1(ast[i], cscope, chunk, body_opts), chunk, nil, ast[i])
      return {chunk = chunk, scope = cscope}
    end
    if (1 == (#ast % 2)) then
      table.insert(ast, utils.sym("nil"))
    else
    end
    for i = 2, (#ast - 1), 2 do
      local condchunk = {}
      local res = compiler.compile1(ast[i], do_scope, condchunk, {nval = 1})
      local cond = res[1]
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
      local fstr
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
      elseif not (branches[(i + 1)]).nested then
        local next_buffer = {}
        compiler.emit(last_buffer, "else", ast)
        compiler.emit(last_buffer, next_buffer, ast)
        compiler.emit(last_buffer, "end", ast)
        last_buffer = next_buffer
      else
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
  SPECIALS["if"] = if_2a
  doc_special("if", {"cond1", "body1", "...", "condN", "bodyN"}, "Conditional form.\nTakes any number of condition/body pairs and evaluates the first body where\nthe condition evaluates to truthy. Similar to cond in other lisps.")
  local function remove_until_condition(bindings)
    if ("until" == bindings[(#bindings - 1)]) then
      table.remove(bindings, (#bindings - 1))
      return table.remove(bindings)
    else
      return nil
    end
  end
  local function compile_until(condition, scope, chunk)
    if condition then
      local _let_414_ = compiler.compile1(condition, scope, chunk, {nval = 1})
      local condition_lua = _let_414_[1]
      return compiler.emit(chunk, ("if %s then break end"):format(tostring(condition_lua)), utils.expr(condition, "expression"))
    else
      return nil
    end
  end
  SPECIALS.each = function(ast, scope, parent)
    compiler.assert((#ast >= 3), "expected body expression", ast[1])
    local binding = compiler.assert(utils["table?"](ast[2]), "expected binding table", ast)
    local _ = compiler.assert((2 <= #binding), "expected binding and iterator", binding)
    local until_condition = remove_until_condition(binding)
    local iter = table.remove(binding, #binding)
    local destructures = {}
    local new_manglings = {}
    local sub_scope = compiler["make-scope"](scope)
    local function destructure_binding(v)
      compiler.assert(("string" ~= type(v)), ("unexpected iterator clause " .. tostring(v)), binding)
      if utils["sym?"](v) then
        return compiler["declare-local"](v, {}, sub_scope, ast, new_manglings)
      else
        local raw = utils.sym(compiler.gensym(sub_scope))
        do end (destructures)[raw] = v
        return compiler["declare-local"](raw, {}, sub_scope, ast)
      end
    end
    local bind_vars = utils.map(binding, destructure_binding)
    local vals = compiler.compile1(iter, scope, parent)
    local val_names = utils.map(vals, tostring)
    local chunk = {}
    compiler.emit(parent, ("for %s in %s do"):format(table.concat(bind_vars, ", "), table.concat(val_names, ", ")), ast)
    for raw, args in utils.stablepairs(destructures) do
      compiler.destructure(args, raw, ast, sub_scope, chunk, {declaration = true, nomulti = true, symtype = "each"})
    end
    compiler["apply-manglings"](sub_scope, new_manglings, ast)
    compile_until(until_condition, sub_scope, chunk)
    compile_do(ast, sub_scope, chunk, 3)
    compiler.emit(parent, chunk, ast)
    return compiler.emit(parent, "end", ast)
  end
  doc_special("each", {"[key value (iterator)]", "..."}, "Runs the body once for each set of values provided by the given iterator.\nMost commonly used with ipairs for sequential tables or pairs for  undefined\norder, but can be used with any iterator.", true)
  local function while_2a(ast, scope, parent)
    local len1 = #parent
    local condition = (compiler.compile1(ast[2], scope, parent, {nval = 1}))[1]
    local len2 = #parent
    local sub_chunk = {}
    if (len1 ~= len2) then
      for i = (len1 + 1), len2 do
        table.insert(sub_chunk, parent[i])
        do end (parent)[i] = nil
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
    local ranges = compiler.assert(utils["table?"](ast[2]), "expected binding table", ast)
    local until_condition = remove_until_condition(ast[2])
    local binding_sym = table.remove(ast[2], 1)
    local sub_scope = compiler["make-scope"](scope)
    local range_args = {}
    local chunk = {}
    compiler.assert(utils["sym?"](binding_sym), ("unable to bind %s %s"):format(type(binding_sym), tostring(binding_sym)), ast[2])
    compiler.assert((#ast >= 3), "expected body expression", ast[1])
    compiler.assert((#ranges <= 3), "unexpected arguments", ranges[4])
    for i = 1, math.min(#ranges, 3) do
      range_args[i] = tostring((compiler.compile1(ranges[i], scope, parent, {nval = 1}))[1])
    end
    compiler.emit(parent, ("for %s = %s do"):format(compiler["declare-local"](binding_sym, {}, sub_scope, ast), table.concat(range_args, ", ")), ast)
    compile_until(until_condition, sub_scope, chunk)
    compile_do(ast, sub_scope, chunk, 3)
    compiler.emit(parent, chunk, ast)
    return compiler.emit(parent, "end", ast)
  end
  SPECIALS["for"] = for_2a
  doc_special("for", {"[index start stop step?]", "..."}, "Numeric loop construct.\nEvaluates body once for each value between start and stop (inclusive).", true)
  local function native_method_call(ast, _scope, _parent, target, args)
    local _let_418_ = ast
    local _ = _let_418_[1]
    local _0 = _let_418_[2]
    local method_string = _let_418_[3]
    local call_string
    if ((target.type == "literal") or (target.type == "varg") or (target.type == "expression")) then
      call_string = "(%s):%s(%s)"
    else
      call_string = "%s:%s(%s)"
    end
    return utils.expr(string.format(call_string, tostring(target), method_string, table.concat(args, ", ")), "statement")
  end
  local function nonnative_method_call(ast, scope, parent, target, args)
    local method_string = tostring((compiler.compile1(ast[3], scope, parent, {nval = 1}))[1])
    local args0 = {tostring(target), unpack(args)}
    return utils.expr(string.format("%s[%s](%s)", tostring(target), method_string, table.concat(args0, ", ")), "statement")
  end
  local function double_eval_protected_method_call(ast, scope, parent, target, args)
    local method_string = tostring((compiler.compile1(ast[3], scope, parent, {nval = 1}))[1])
    local call = "(function(tgt, m, ...) return tgt[m](tgt, ...) end)(%s, %s)"
    table.insert(args, 1, method_string)
    return utils.expr(string.format(call, tostring(target), table.concat(args, ", ")), "statement")
  end
  local function method_call(ast, scope, parent)
    compiler.assert((2 < #ast), "expected at least 2 arguments", ast)
    local _let_420_ = compiler.compile1(ast[2], scope, parent, {nval = 1})
    local target = _let_420_[1]
    local args = {}
    for i = 4, #ast do
      local subexprs
      local _421_
      if (i ~= #ast) then
        _421_ = 1
      else
        _421_ = nil
      end
      subexprs = compiler.compile1(ast[i], scope, parent, {nval = _421_})
      utils.map(subexprs, tostring, args)
    end
    if ((type(ast[3]) == "string") and utils["valid-lua-identifier?"](ast[3])) then
      return native_method_call(ast, scope, parent, target, args)
    elseif (target.type == "sym") then
      return nonnative_method_call(ast, scope, parent, target, args)
    else
      return double_eval_protected_method_call(ast, scope, parent, target, args)
    end
  end
  SPECIALS[":"] = method_call
  doc_special(":", {"tbl", "method-name", "..."}, "Call the named method on tbl with the provided args.\nMethod name doesn't have to be known at compile-time; if it is, use\n(tbl:method-name ...) instead.")
  SPECIALS.comment = function(ast, _, parent)
    local els = {}
    for i = 2, #ast do
      table.insert(els, view(ast[i], {["one-line?"] = true}))
    end
    return compiler.emit(parent, ("--[[ " .. table.concat(els, " ") .. " ]]--"), ast)
  end
  doc_special("comment", {"..."}, "Comment which will be emitted in Lua output.", true)
  local function hashfn_max_used(f_scope, i, max)
    local max0
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
    local f_scope
    do
      local _426_ = compiler["make-scope"](scope)
      do end (_426_)["vararg"] = false
      _426_["hashfn"] = true
      f_scope = _426_
    end
    local f_chunk = {}
    local name = compiler.gensym(scope)
    local symbol = utils.sym(name)
    local args = {}
    compiler["declare-local"](symbol, {}, scope, ast)
    for i = 1, 9 do
      args[i] = compiler["declare-local"](utils.sym(("$" .. i)), {}, f_scope, ast)
    end
    local function walker(idx, node, parent_node)
      if (utils["sym?"](node) and (tostring(node) == "$...")) then
        parent_node[idx] = utils.varg()
        f_scope.vararg = true
        return nil
      else
        return (utils["list?"](node) or utils["table?"](node))
      end
    end
    utils["walk-tree"](ast[2], walker)
    compiler.compile1(ast[2], f_scope, f_chunk, {tail = true})
    local max_used = hashfn_max_used(f_scope, 1, 0)
    if f_scope.vararg then
      compiler.assert((max_used == 0), "$ and $... in hashfn are mutually exclusive", ast)
    else
    end
    local arg_str
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
  local function arithmetic_special(name, zero_arity, unary_prefix, ast, scope, parent)
    local len = #ast
    local operands = {}
    local padded_op = (" " .. name .. " ")
    for i = 2, len do
      local subexprs = compiler.compile1(ast[i], scope, parent)
      if (i == len) then
        utils.map(subexprs, tostring, operands)
      else
        table.insert(operands, tostring(subexprs[1]))
      end
    end
    local _431_ = #operands
    if (_431_ == 0) then
      local _433_
      do
        local _432_ = zero_arity
        compiler.assert(_432_, "Expected more than 0 arguments", ast)
        _433_ = _432_
      end
      return utils.expr(_433_, "literal")
    elseif (_431_ == 1) then
      if unary_prefix then
        return ("(" .. unary_prefix .. padded_op .. operands[1] .. ")")
      else
        return operands[1]
      end
    elseif true then
      local _ = _431_
      return ("(" .. table.concat(operands, padded_op) .. ")")
    else
      return nil
    end
  end
  local function define_arithmetic_special(name, zero_arity, unary_prefix, _3flua_name)
    local _439_
    do
      local _436_ = (_3flua_name or name)
      local _437_ = zero_arity
      local _438_ = unary_prefix
      local function _440_(...)
        return arithmetic_special(_436_, _437_, _438_, ...)
      end
      _439_ = _440_
    end
    SPECIALS[name] = _439_
    return doc_special(name, {"a", "b", "..."}, "Arithmetic operator; works the same as Lua but accepts more arguments.")
  end
  define_arithmetic_special("+", "0")
  define_arithmetic_special("..", "''")
  define_arithmetic_special("^")
  define_arithmetic_special("-", nil, "")
  define_arithmetic_special("*", "1")
  define_arithmetic_special("%")
  define_arithmetic_special("/", nil, "1")
  define_arithmetic_special("//", nil, "1")
  SPECIALS["or"] = function(ast, scope, parent)
    return arithmetic_special("or", "false", nil, ast, scope, parent)
  end
  SPECIALS["and"] = function(ast, scope, parent)
    return arithmetic_special("and", "true", nil, ast, scope, parent)
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
        local subexprs
        local _441_
        if (i ~= len) then
          _441_ = 1
        else
          _441_ = nil
        end
        subexprs = compiler.compile1(ast[i], scope, parent, {nval = _441_})
        utils.map(subexprs, tostring, operands)
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
    local _451_
    do
      local _447_ = native
      local _448_ = name
      local _449_ = zero_arity
      local _450_ = unary_prefix
      local function _452_(...)
        return bitop_special(_447_, _448_, _449_, _450_, ...)
      end
      _451_ = _452_
    end
    SPECIALS[name] = _451_
    return nil
  end
  define_bitop_special("lshift", nil, "1", "<<")
  define_bitop_special("rshift", nil, "1", ">>")
  define_bitop_special("band", "0", "0", "&")
  define_bitop_special("bor", "0", "0", "|")
  define_bitop_special("bxor", "0", "0", "~")
  doc_special("lshift", {"x", "n"}, "Bitwise logical left shift of x by n bits.\nOnly works in Lua 5.3+ or LuaJIT with the --use-bit-lib flag.")
  doc_special("rshift", {"x", "n"}, "Bitwise logical right shift of x by n bits.\nOnly works in Lua 5.3+ or LuaJIT with the --use-bit-lib flag.")
  doc_special("band", {"x1", "x2", "..."}, "Bitwise AND of any number of arguments.\nOnly works in Lua 5.3+ or LuaJIT with the --use-bit-lib flag.")
  doc_special("bor", {"x1", "x2", "..."}, "Bitwise OR of any number of arguments.\nOnly works in Lua 5.3+ or LuaJIT with the --use-bit-lib flag.")
  doc_special("bxor", {"x1", "x2", "..."}, "Bitwise XOR of any number of arguments.\nOnly works in Lua 5.3+ or LuaJIT with the --use-bit-lib flag.")
  doc_special("..", {"a", "b", "..."}, "String concatenation operator; works the same as Lua but accepts more arguments.")
  local function native_comparator(op, _453_, scope, parent)
    local _arg_454_ = _453_
    local _ = _arg_454_[1]
    local lhs_ast = _arg_454_[2]
    local rhs_ast = _arg_454_[3]
    local _let_455_ = compiler.compile1(lhs_ast, scope, parent, {nval = 1})
    local lhs = _let_455_[1]
    local _let_456_ = compiler.compile1(rhs_ast, scope, parent, {nval = 1})
    local rhs = _let_456_[1]
    return string.format("(%s %s %s)", tostring(lhs), op, tostring(rhs))
  end
  local function double_eval_protected_comparator(op, chain_op, ast, scope, parent)
    local arglist = {}
    local comparisons = {}
    local vals = {}
    local chain = string.format(" %s ", (chain_op or "and"))
    for i = 2, #ast do
      table.insert(arglist, tostring(compiler.gensym(scope)))
      table.insert(vals, tostring((compiler.compile1(ast[i], scope, parent, {nval = 1}))[1]))
    end
    for i = 1, (#arglist - 1) do
      table.insert(comparisons, string.format("(%s %s %s)", arglist[i], op, arglist[(i + 1)]))
    end
    return string.format("(function(%s) return %s end)(%s)", table.concat(arglist, ","), table.concat(comparisons, chain), table.concat(vals, ","))
  end
  local function define_comparator_special(name, _3flua_op, _3fchain_op)
    do
      local op = (_3flua_op or name)
      local function opfn(ast, scope, parent)
        compiler.assert((2 < #ast), "expected at least two arguments", ast)
        if (3 == #ast) then
          return native_comparator(op, ast, scope, parent)
        else
          return double_eval_protected_comparator(op, _3fchain_op, ast, scope, parent)
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
      return ((_3frealop or op) .. tostring(tail[1]))
    end
    SPECIALS[op] = opfn
    return nil
  end
  define_unary_special("not", "not ")
  doc_special("not", {"x"}, "Logical operator; works the same as Lua.")
  define_unary_special("bnot", "~")
  doc_special("bnot", {"x"}, "Bitwise negation; only works in Lua 5.3+ or LuaJIT with the --use-bit-lib flag.")
  define_unary_special("length", "#")
  doc_special("length", {"x"}, "Returns the length of a table or string.")
  do end (SPECIALS)["~="] = SPECIALS["not="]
  SPECIALS["#"] = SPECIALS.length
  SPECIALS.quote = function(ast, scope, parent)
    compiler.assert((#ast == 2), "expected one argument", ast)
    local runtime, this_scope = true, scope
    while this_scope do
      this_scope = this_scope.parent
      if (this_scope == compiler.scopes.compiler) then
        runtime = false
      else
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
    return {table = utils.copy(table), math = utils.copy(math), string = utils.copy(string), pairs = pairs, ipairs = ipairs, select = select, tostring = tostring, tonumber = tonumber, bit = rawget(_G, "bit"), pcall = pcall, xpcall = xpcall, next = next, print = print, type = type, assert = assert, error = error, setmetatable = setmetatable, getmetatable = safe_getmetatable, require = safe_require, rawlen = rawget(_G, "rawlen"), rawget = rawget, rawset = rawset, rawequal = rawequal, _VERSION = _VERSION}
  end
  local function combined_mt_pairs(env)
    local combined = {}
    local _let_459_ = getmetatable(env)
    local __index = _let_459_["__index"]
    if ("table" == type(__index)) then
      for k, v in pairs(__index) do
        combined[k] = v
      end
    else
    end
    for k, v in next, env, nil do
      combined[k] = v
    end
    return next, combined, nil
  end
  local function make_compiler_env(ast, scope, parent, _3fopts)
    local provided
    do
      local _461_ = (_3fopts or utils.root.options)
      if ((_G.type(_461_) == "table") and ((_461_)["compiler-env"] == "strict")) then
        provided = safe_compiler_env()
      elseif ((_G.type(_461_) == "table") and (nil ~= (_461_).compilerEnv)) then
        local compilerEnv = (_461_).compilerEnv
        provided = compilerEnv
      elseif ((_G.type(_461_) == "table") and (nil ~= (_461_)["compiler-env"])) then
        local compiler_env = (_461_)["compiler-env"]
        provided = compiler_env
      elseif true then
        local _ = _461_
        provided = safe_compiler_env(false)
      else
        provided = nil
      end
    end
    local env
    local function _463_(base)
      return utils.sym(compiler.gensym((compiler.scopes.macro or scope), base))
    end
    local function _464_()
      return compiler.scopes.macro
    end
    local function _465_(symbol)
      compiler.assert(compiler.scopes.macro, "must call from macro", ast)
      return compiler.scopes.macro.manglings[tostring(symbol)]
    end
    local function _466_(form)
      compiler.assert(compiler.scopes.macro, "must call from macro", ast)
      return compiler.macroexpand(form, compiler.scopes.macro)
    end
    env = {_AST = ast, _CHUNK = parent, _IS_COMPILER = true, _SCOPE = scope, _SPECIALS = compiler.scopes.global.specials, _VARARG = utils.varg(), ["macro-loaded"] = macro_loaded, unpack = unpack, ["assert-compile"] = compiler.assert, view = view, version = utils.version, metadata = compiler.metadata, list = utils.list, ["list?"] = utils["list?"], ["table?"] = utils["table?"], sequence = utils.sequence, ["sequence?"] = utils["sequence?"], sym = utils.sym, ["sym?"] = utils["sym?"], ["multi-sym?"] = utils["multi-sym?"], comment = utils.comment, ["comment?"] = utils["comment?"], ["varg?"] = utils["varg?"], gensym = _463_, ["get-scope"] = _464_, ["in-scope?"] = _465_, macroexpand = _466_}
    env._G = env
    return setmetatable(env, {__index = provided, __newindex = provided, __pairs = combined_mt_pairs})
  end
  local function _468_(...)
    local tbl_14_auto = {}
    local i_15_auto = #tbl_14_auto
    for c in string.gmatch((package.config or ""), "([^\n]+)") do
      local val_16_auto = c
      if (nil ~= val_16_auto) then
        i_15_auto = (i_15_auto + 1)
        do end (tbl_14_auto)[i_15_auto] = val_16_auto
      else
      end
    end
    return tbl_14_auto
  end
  local _local_467_ = _468_(...)
  local dirsep = _local_467_[1]
  local pathsep = _local_467_[2]
  local pathmark = _local_467_[3]
  local pkg_config = {dirsep = (dirsep or "/"), pathmark = (pathmark or ";"), pathsep = (pathsep or "?")}
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
      local _470_ = (io.open(filename) or io.open(filename2))
      if (nil ~= _470_) then
        local file = _470_
        file:close()
        return filename
      else
        return nil
      end
    end
    local function find_in_path(start)
      local _472_ = fullpath:match(pattern, start)
      if (nil ~= _472_) then
        local path = _472_
        return (try_path(path) or find_in_path((start + #path + 1)))
      else
        return nil
      end
    end
    return find_in_path(1)
  end
  local function make_searcher(_3foptions)
    local function _474_(module_name)
      local opts = utils.copy(utils.root.options)
      for k, v in pairs((_3foptions or {})) do
        opts[k] = v
      end
      opts["module-name"] = module_name
      local _475_ = search_module(module_name)
      if (nil ~= _475_) then
        local filename = _475_
        local _478_
        do
          local _476_ = filename
          local _477_ = opts
          local function _479_(...)
            return utils["fennel-module"].dofile(_476_, _477_, ...)
          end
          _478_ = _479_
        end
        return _478_, filename
      else
        return nil
      end
    end
    return _474_
  end
  local function fennel_macro_searcher(module_name)
    local opts
    do
      local _481_ = utils.copy(utils.root.options)
      do end (_481_)["env"] = "_COMPILER"
      _481_["requireAsInclude"] = false
      _481_["allowedGlobals"] = nil
      opts = _481_
    end
    local _482_ = search_module(module_name, utils["fennel-module"]["macro-path"])
    if (nil ~= _482_) then
      local filename = _482_
      local _485_
      do
        local _483_ = filename
        local _484_ = opts
        local function _486_(...)
          return utils["fennel-module"].dofile(_483_, _484_, ...)
        end
        _485_ = _486_
      end
      return _485_, filename
    else
      return nil
    end
  end
  local function lua_macro_searcher(module_name)
    local _488_ = search_module(module_name, package.path)
    if (nil ~= _488_) then
      local filename = _488_
      local code
      do
        local f = io.open(filename)
        local function close_handlers_8_auto(ok_9_auto, ...)
          f:close()
          if ok_9_auto then
            return ...
          else
            return error(..., 0)
          end
        end
        local function _490_()
          return assert(f:read("*a"))
        end
        code = close_handlers_8_auto(_G.xpcall(_490_, (package.loaded.fennel or debug).traceback))
      end
      local chunk = load_code(code, make_compiler_env(), filename)
      return chunk, filename
    else
      return nil
    end
  end
  local macro_searchers = {fennel_macro_searcher, lua_macro_searcher}
  local function search_macro_module(modname, n)
    local _492_ = macro_searchers[n]
    if (nil ~= _492_) then
      local f = _492_
      local _493_, _494_ = f(modname)
      if ((nil ~= _493_) and true) then
        local loader = _493_
        local _3ffilename = _494_
        return loader, _3ffilename
      elseif true then
        local _ = _493_
        return search_macro_module(modname, (n + 1))
      else
        return nil
      end
    else
      return nil
    end
  end
  local function metadata_only_fennel(modname)
    if ((modname == "fennel.macros") or (package and package.loaded and ("table" == type(package.loaded[modname])) and (package.loaded[modname].metadata == compiler.metadata))) then
      return {metadata = compiler.metadata}
    else
      return nil
    end
  end
  local function _498_(modname)
    local function _499_()
      local loader, filename = search_macro_module(modname, 1)
      compiler.assert(loader, (modname .. " module not found."))
      do end (macro_loaded)[modname] = loader(modname, filename)
      return macro_loaded[modname]
    end
    return (macro_loaded[modname] or metadata_only_fennel(modname) or _499_())
  end
  safe_require = _498_
  local function add_macros(macros_2a, ast, scope)
    compiler.assert(utils["table?"](macros_2a), "expected macros to be table", ast)
    for k, v in pairs(macros_2a) do
      compiler.assert((type(v) == "function"), "expected each macro to be function", ast)
      do end (scope.macros)[k] = v
    end
    return nil
  end
  local function resolve_module_name(_500_, _scope, _parent, opts)
    local _arg_501_ = _500_
    local filename = _arg_501_["filename"]
    local second = _arg_501_[2]
    local filename0 = (filename or (utils["table?"](second) and second.filename))
    local module_name = utils.root.options["module-name"]
    local modexpr = compiler.compile(second, opts)
    local modname_chunk = load_code(modexpr)
    return modname_chunk(module_name, filename0)
  end
  SPECIALS["require-macros"] = function(ast, scope, parent, _3freal_ast)
    compiler.assert((#ast == 2), "Expected one module name argument", (_3freal_ast or ast))
    local modname = resolve_module_name(ast, scope, parent, {})
    compiler.assert(("string" == type(modname)), "module name must compile to string", (_3freal_ast or ast))
    if not macro_loaded[modname] then
      local loader, filename = search_macro_module(modname, 1)
      compiler.assert(loader, (modname .. " module not found."), ast)
      do end (macro_loaded)[modname] = loader(modname, filename)
    else
    end
    if ("import-macros" == tostring(ast[1])) then
      return macro_loaded[modname]
    else
      return add_macros(macro_loaded[modname], ast, scope, parent)
    end
  end
  doc_special("require-macros", {"macro-module-name"}, "Load given module and use its contents as macro definitions in current scope.\nMacro module should return a table of macro functions with string keys.\nConsider using import-macros instead as it is more flexible.")
  local function emit_included_fennel(src, path, opts, sub_chunk)
    local subscope = compiler["make-scope"](utils.root.scope.parent)
    local forms = {}
    if utils.root.options.requireAsInclude then
      subscope.specials.require = compiler["require-include"]
    else
    end
    for _, val in parser.parser(parser["string-stream"](src), path) do
      table.insert(forms, val)
    end
    for i = 1, #forms do
      local subopts
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
    local src
    do
      local f = assert(io.open(path))
      local function close_handlers_8_auto(ok_9_auto, ...)
        f:close()
        if ok_9_auto then
          return ...
        else
          return error(..., 0)
        end
      end
      local function _507_()
        return f:read("*all"):gsub("[\13\n]*$", "")
      end
      src = close_handlers_8_auto(_G.xpcall(_507_, (package.loaded.fennel or debug).traceback))
    end
    local ret = utils.expr(("require(\"" .. mod .. "\")"), "statement")
    local target = ("package.preload[%q]"):format(mod)
    local preload_str = (target .. " = " .. target .. " or function(...)")
    local temp_chunk, sub_chunk = {}, {}
    compiler.emit(temp_chunk, preload_str, ast)
    compiler.emit(temp_chunk, sub_chunk)
    compiler.emit(temp_chunk, "end", ast)
    for i, v in ipairs(temp_chunk) do
      table.insert(utils.root.chunk, i, v)
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
    else
      return nil
    end
  end
  SPECIALS.include = function(ast, scope, parent, opts)
    compiler.assert((#ast == 2), "expected one argument", ast)
    local modexpr
    do
      local _510_, _511_ = pcall(resolve_module_name, ast, scope, parent, opts)
      if ((_510_ == true) and (nil ~= _511_)) then
        local modname = _511_
        modexpr = utils.expr(string.format("%q", modname), "literal")
      elseif true then
        local _ = _510_
        modexpr = (compiler.compile1(ast[2], scope, parent, {nval = 1}))[1]
      else
        modexpr = nil
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
      local _
      utils.root.options["module-name"] = mod
      _ = nil
      local res
      local function _515_()
        local _514_ = search_module(mod)
        if (nil ~= _514_) then
          local fennel_path = _514_
          return include_path(ast, opts, fennel_path, mod, true)
        elseif true then
          local _0 = _514_
          local lua_path = search_module(mod, package.path)
          if lua_path then
            return include_path(ast, opts, lua_path, mod, false)
          elseif opts.fallback then
            return opts.fallback(modexpr)
          else
            return compiler.assert(false, ("module not found " .. mod), ast)
          end
        else
          return nil
        end
      end
      res = ((utils["member?"](mod, (utils.root.options.skipInclude or {})) and utils.expr("nil --[[SKIPPED INCLUDE]]--", "literal")) or include_circular_fallback(mod, modexpr, opts.fallback, ast) or utils.root.scope.includes[mod] or _515_())
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
    return load_code(compiler.compile(ast, opts), wrap_env(env))(opts["module-name"], ast.filename)
  end
  SPECIALS.macros = function(ast, scope, parent)
    compiler.assert((#ast == 2), "Expected one table argument", ast)
    return add_macros(eval_compiler_2a(ast[2], scope, parent), ast, scope, parent)
  end
  doc_special("macros", {"{:macro-name-1 (fn [...] ...) ... :macro-name-N macro-body-N}"}, "Define all functions in the given table as macros local to the current scope.")
  SPECIALS["eval-compiler"] = function(ast, scope, parent)
    local old_first = ast[1]
    ast[1] = utils.sym("do")
    local val = eval_compiler_2a(ast, scope, parent)
    do end (ast)[1] = old_first
    return val
  end
  doc_special("eval-compiler", {"..."}, "Evaluate the body at compile-time. Use the macro system instead if possible.", true)
  return {doc = doc_2a, ["current-global-names"] = current_global_names, ["load-code"] = load_code, ["macro-loaded"] = macro_loaded, ["macro-searchers"] = macro_searchers, ["make-compiler-env"] = make_compiler_env, ["search-module"] = search_module, ["make-searcher"] = make_searcher, ["wrap-env"] = wrap_env}
end
package.preload["fennel.compiler"] = package.preload["fennel.compiler"] or function(...)
  local utils = require("fennel.utils")
  local parser = require("fennel.parser")
  local friend = require("fennel.friend")
  local unpack = (table.unpack or _G.unpack)
  local scopes = {}
  local function make_scope(_3fparent)
    local parent = (_3fparent or scopes.global)
    local _203_
    if parent then
      _203_ = ((parent.depth or 0) + 1)
    else
      _203_ = 0
    end
    return {includes = setmetatable({}, {__index = (parent and parent.includes)}), macros = setmetatable({}, {__index = (parent and parent.macros)}), manglings = setmetatable({}, {__index = (parent and parent.manglings)}), specials = setmetatable({}, {__index = (parent and parent.specials)}), symmeta = setmetatable({}, {__index = (parent and parent.symmeta)}), unmanglings = setmetatable({}, {__index = (parent and parent.unmanglings)}), gensyms = setmetatable({}, {__index = (parent and parent.gensyms)}), autogensyms = setmetatable({}, {__index = (parent and parent.autogensyms)}), vararg = (parent and parent.vararg), depth = _203_, hashfn = (parent and parent.hashfn), refedglobals = {}, parent = parent}
  end
  local function assert_msg(ast, msg)
    local ast_tbl
    if ("table" == type(ast)) then
      ast_tbl = ast
    else
      ast_tbl = {}
    end
    local m = getmetatable(ast)
    local filename = ((m and m.filename) or ast_tbl.filename or "unknown")
    local line = ((m and m.line) or ast_tbl.line or "?")
    local target = tostring((utils["sym?"](ast_tbl[1]) or ast_tbl[1] or "()"))
    return string.format("%s:%s: Compile error in '%s': %s", filename, line, target, msg)
  end
  local function assert_compile(condition, msg, ast)
    if not condition then
      local _let_206_ = (utils.root.options or {})
      local source = _let_206_["source"]
      local unfriendly = _let_206_["unfriendly"]
      if (nil == utils.hook("assert-compile", condition, msg, ast, utils.root.reset)) then
        utils.root.reset()
        if (unfriendly or not friend or not _G.io or not _G.io.read) then
          error(assert_msg(ast, msg), 0)
        else
          friend["assert-compile"](condition, msg, ast, source)
        end
      else
      end
    else
    end
    return condition
  end
  scopes.global = make_scope()
  scopes.global.vararg = true
  scopes.compiler = make_scope(scopes.global)
  scopes.macro = scopes.global
  local serialize_subst = {["\7"] = "\\a", ["\8"] = "\\b", ["\9"] = "\\t", ["\n"] = "n", ["\11"] = "\\v", ["\12"] = "\\f"}
  local function serialize_string(str)
    local function _210_(_241)
      return ("\\" .. _241:byte())
    end
    return string.gsub(string.gsub(string.format("%q", str), ".", serialize_subst), "[\128-\255]", _210_)
  end
  local function global_mangling(str)
    if utils["valid-lua-identifier?"](str) then
      return str
    else
      local function _211_(_241)
        return string.format("_%02x", _241:byte())
      end
      return ("__fnl_global__" .. str:gsub("[^%w]", _211_))
    end
  end
  local function global_unmangling(identifier)
    local _213_ = string.match(identifier, "^__fnl_global__(.*)$")
    if (nil ~= _213_) then
      local rest = _213_
      local _214_
      local function _215_(_241)
        return string.char(tonumber(_241:sub(2), 16))
      end
      _214_ = string.gsub(rest, "_[%da-f][%da-f]", _215_)
      return _214_
    elseif true then
      local _ = _213_
      return identifier
    else
      return nil
    end
  end
  local allowed_globals = nil
  local function global_allowed_3f(name)
    return (not allowed_globals or utils["member?"](name, allowed_globals))
  end
  local function unique_mangling(original, mangling, scope, append)
    if (scope.unmanglings[mangling] and not scope.gensyms[mangling]) then
      return unique_mangling(original, (original .. append), scope, (append + 1))
    else
      return mangling
    end
  end
  local function local_mangling(str, scope, ast, _3ftemp_manglings)
    assert_compile(not utils["multi-sym?"](str), ("unexpected multi symbol " .. str), ast)
    local raw
    if ((utils["lua-keywords"])[str] or str:match("^%d")) then
      raw = ("_" .. str)
    else
      raw = str
    end
    local mangling
    local function _219_(_241)
      return string.format("_%02x", _241:byte())
    end
    mangling = string.gsub(string.gsub(raw, "-", "_"), "[^%w_]", _219_)
    local unique = unique_mangling(mangling, mangling, scope, 0)
    do end (scope.unmanglings)[unique] = str
    do
      local manglings = (_3ftemp_manglings or scope.manglings)
      do end (manglings)[str] = unique
    end
    return unique
  end
  local function apply_manglings(scope, new_manglings, ast)
    for raw, mangled in pairs(new_manglings) do
      assert_compile(not scope.refedglobals[mangled], ("use of global " .. raw .. " is aliased by a local"), ast)
      do end (scope.manglings)[raw] = mangled
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
  local function next_append()
    utils.root.scope["gensym-append"] = ((utils.root.scope["gensym-append"] or 0) + 1)
    return ("_" .. utils.root.scope["gensym-append"] .. "_")
  end
  local function gensym(scope, _3fbase, _3fsuffix)
    local mangling = ((_3fbase or "") .. next_append() .. (_3fsuffix or ""))
    while scope.unmanglings[mangling] do
      mangling = ((_3fbase or "") .. next_append() .. (_3fsuffix or ""))
    end
    scope.unmanglings[mangling] = (_3fbase or true)
    do end (scope.gensyms)[mangling] = true
    return mangling
  end
  local function autogensym(base, scope)
    local _222_ = utils["multi-sym?"](base)
    if (nil ~= _222_) then
      local parts = _222_
      parts[1] = autogensym(parts[1], scope)
      return table.concat(parts, ((parts["multi-sym-method-call"] and ":") or "."))
    elseif true then
      local _ = _222_
      local function _223_()
        local mangling = gensym(scope, base:sub(1, ( - 2)), "auto")
        do end (scope.autogensyms)[base] = mangling
        return mangling
      end
      return (scope.autogensyms[base] or _223_())
    else
      return nil
    end
  end
  local function check_binding_valid(symbol, scope, ast)
    local name = tostring(symbol)
    assert_compile(not name:find("&"), "illegal character &")
    assert_compile(not (scope.specials[name] or scope.macros[name]), ("local %s was overshadowed by a special form or macro"):format(name), ast)
    return assert_compile(not utils["quoted?"](symbol), string.format("macro tried to bind %s without gensym", name), symbol)
  end
  local function declare_local(symbol, meta, scope, ast, _3ftemp_manglings)
    check_binding_valid(symbol, scope, ast)
    local name = tostring(symbol)
    assert_compile(not utils["multi-sym?"](name), ("unexpected multi symbol " .. name), ast)
    do end (scope.symmeta)[name] = meta
    return local_mangling(name, scope, ast, _3ftemp_manglings)
  end
  local function hashfn_arg_name(name, multi_sym_parts, scope)
    if not scope.hashfn then
      return nil
    elseif (name == "$") then
      return "$1"
    elseif multi_sym_parts then
      if (multi_sym_parts and (multi_sym_parts[1] == "$")) then
        multi_sym_parts[1] = "$1"
      else
      end
      return table.concat(multi_sym_parts, ".")
    else
      return nil
    end
  end
  local function symbol_to_expression(symbol, scope, _3freference_3f)
    utils.hook("symbol-to-expression", symbol, scope, _3freference_3f)
    local name = symbol[1]
    local multi_sym_parts = utils["multi-sym?"](name)
    local name0 = (hashfn_arg_name(name, multi_sym_parts, scope) or name)
    local parts = (multi_sym_parts or {name0})
    local etype = (((#parts > 1) and "expression") or "sym")
    local local_3f = scope.manglings[parts[1]]
    if (local_3f and scope.symmeta[parts[1]]) then
      scope.symmeta[parts[1]]["used"] = true
    else
    end
    assert_compile(not scope.macros[parts[1]], "tried to reference a macro at runtime", symbol)
    assert_compile((not _3freference_3f or local_3f or ("_ENV" == parts[1]) or global_allowed_3f(parts[1])), ("unknown identifier in strict mode: " .. tostring(parts[1])), symbol)
    if (allowed_globals and not local_3f and scope.parent) then
      scope.parent.refedglobals[parts[1]] = true
    else
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
    elseif ((#chunk >= 3) and ((chunk[(#chunk - 2)]).leaf == "do") and not (chunk[(#chunk - 1)]).leaf and (chunk[#chunk].leaf == "end")) then
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
      return utils.map(chunk, peephole)
    end
  end
  local function flatten_chunk_correlated(main_chunk, options)
    local function flatten(chunk, out, last_line, file)
      local last_line0 = last_line
      if chunk.leaf then
        out[last_line0] = ((out[last_line0] or "") .. " " .. chunk.leaf)
      else
        for _, subchunk in ipairs(chunk) do
          if (subchunk.leaf or (#subchunk > 0)) then
            local source = utils["ast-source"](subchunk.ast)
            if (file == source.filename) then
              last_line0 = math.max(last_line0, (source.line or 0))
            else
            end
            last_line0 = flatten(subchunk, out, last_line0, file)
          else
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
      else
      end
    end
    return table.concat(out, "\n")
  end
  local function flatten_chunk(sm, chunk, tab, depth)
    if chunk.leaf then
      local code = chunk.leaf
      local info = chunk.ast
      if sm then
        table.insert(sm, {(info and info.filename), (info and info.line)})
      else
      end
      return code
    else
      local tab0
      do
        local _236_ = tab
        if (_236_ == true) then
          tab0 = "  "
        elseif (_236_ == false) then
          tab0 = ""
        elseif (_236_ == tab) then
          tab0 = tab
        elseif (_236_ == nil) then
          tab0 = ""
        else
          tab0 = nil
        end
      end
      local function parter(c)
        if (c.leaf or (#c > 0)) then
          local sub = flatten_chunk(sm, c, tab0, (depth + 1))
          if (depth > 0) then
            return (tab0 .. sub:gsub("\n", ("\n" .. tab0)))
          else
            return sub
          end
        else
          return nil
        end
      end
      return table.concat(utils.map(chunk, parter), "\n")
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
      local sm = {}
      local ret = flatten_chunk(sm, chunk0, options.indent, 0)
      if sm then
        sm.short_src = (options.filename or make_short_src((options.source or ret)))
        if options.filename then
          sm.key = ("@" .. options.filename)
        else
          sm.key = ret
        end
        sourcemap[sm.key] = sm
      else
      end
      return ret, sm
    end
  end
  local function make_metadata()
    local function _245_(self, tgt, key)
      if self[tgt] then
        return self[tgt][key]
      else
        return nil
      end
    end
    local function _247_(self, tgt, key, value)
      self[tgt] = (self[tgt] or {})
      do end (self[tgt])[key] = value
      return tgt
    end
    local function _248_(self, tgt, ...)
      local kv_len = select("#", ...)
      local kvs = {...}
      if ((kv_len % 2) ~= 0) then
        error("metadata:setall() expected even number of k/v pairs")
      else
      end
      self[tgt] = (self[tgt] or {})
      for i = 1, kv_len, 2 do
        self[tgt][kvs[i]] = kvs[(i + 1)]
      end
      return tgt
    end
    return setmetatable({}, {__index = {get = _245_, set = _247_, setall = _248_}, __mode = "k"})
  end
  local function exprs1(exprs)
    return table.concat(utils.map(exprs, tostring), ", ")
  end
  local function keep_side_effects(exprs, chunk, start, ast)
    local start0 = (start or 1)
    for j = start0, #exprs do
      local se = exprs[j]
      if ((se.type == "expression") and (se[1] ~= "nil")) then
        emit(chunk, string.format("do local _ = %s end", tostring(se)), ast)
      elseif (se.type == "statement") then
        local code = tostring(se)
        local disambiguated
        if (code:byte() == 40) then
          disambiguated = ("do end " .. code)
        else
          disambiguated = code
        end
        emit(chunk, disambiguated, ast)
      else
      end
    end
    return nil
  end
  local function handle_compile_opts(exprs, parent, opts, ast)
    if opts.nval then
      local n = opts.nval
      local len = #exprs
      if (n ~= len) then
        if (len > n) then
          keep_side_effects(exprs, parent, (n + 1), ast)
          for i = (n + 1), len do
            exprs[i] = nil
          end
        else
          for i = (#exprs + 1), n do
            exprs[i] = utils.expr("nil", "literal")
          end
        end
      else
      end
    else
    end
    if opts.tail then
      emit(parent, string.format("return %s", exprs1(exprs)), ast)
    else
    end
    if opts.target then
      local result = exprs1(exprs)
      local function _256_()
        if (result == "") then
          return "nil"
        else
          return result
        end
      end
      emit(parent, string.format("%s = %s", opts.target, _256_()), ast)
    else
    end
    if (opts.tail or opts.target) then
      return {returned = true}
    else
      local _258_ = exprs
      _258_["returned"] = true
      return _258_
    end
  end
  local function find_macro(ast, scope, multi_sym_parts)
    local function find_in_table(t, i)
      if (i <= #multi_sym_parts) then
        return find_in_table((utils["table?"](t) and t[multi_sym_parts[i]]), (i + 1))
      else
        return t
      end
    end
    local macro_2a = (utils["sym?"](ast[1]) and scope.macros[tostring(ast[1])])
    if (not macro_2a and multi_sym_parts) then
      local nested_macro = find_in_table(scope.macros, 1)
      assert_compile((not scope.macros[multi_sym_parts[1]] or (type(nested_macro) == "function")), "macro not found in imported macro module", ast)
      return nested_macro
    else
      return macro_2a
    end
  end
  local function propagate_trace_info(_262_, _index, node)
    local _arg_263_ = _262_
    local filename = _arg_263_["filename"]
    local line = _arg_263_["line"]
    local bytestart = _arg_263_["bytestart"]
    local byteend = _arg_263_["byteend"]
    if (("table" == type(node)) and (filename ~= node.filename)) then
      local src = utils["ast-source"](node)
      src.filename, src.line = filename, line
      src.bytestart, src.byteend = bytestart, byteend
    else
    end
    return ("table" == type(node))
  end
  local function macroexpand_2a(ast, scope, _3fonce)
    local _265_
    if utils["list?"](ast) then
      _265_ = find_macro(ast, scope, utils["multi-sym?"](ast[1]))
    else
      _265_ = nil
    end
    if (_265_ == false) then
      return ast
    elseif (nil ~= _265_) then
      local macro_2a = _265_
      local old_scope = scopes.macro
      local _
      scopes.macro = scope
      _ = nil
      local ok, transformed = nil, nil
      local function _267_()
        return macro_2a(unpack(ast, 2))
      end
      ok, transformed = xpcall(_267_, debug.traceback)
      local function _269_()
        local _268_ = ast
        local function _270_(...)
          return propagate_trace_info(_268_, ...)
        end
        return _270_
      end
      utils["walk-tree"](transformed, _269_())
      scopes.macro = old_scope
      assert_compile(ok, transformed, ast)
      if (_3fonce or not transformed) then
        return transformed
      else
        return macroexpand_2a(transformed, scope)
      end
    elseif true then
      local _ = _265_
      return ast
    else
      return nil
    end
  end
  local function compile_special(ast, scope, parent, opts, special)
    local exprs = (special(ast, scope, parent, opts) or utils.expr("nil", "literal"))
    local exprs0
    if ("table" ~= type(exprs)) then
      exprs0 = utils.expr(exprs, "expression")
    else
      exprs0 = exprs
    end
    local exprs2
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
  local function compile_function_call(ast, scope, parent, opts, compile1, len)
    local fargs = {}
    local fcallee = (compile1(ast[1], scope, parent, {nval = 1}))[1]
    assert_compile((("string" == type(ast[1])) or (fcallee.type ~= "literal")), ("cannot call literal value " .. tostring(ast[1])), ast)
    for i = 2, len do
      local subexprs
      local _276_
      if (i ~= len) then
        _276_ = 1
      else
        _276_ = nil
      end
      subexprs = compile1(ast[i], scope, parent, {nval = _276_})
      table.insert(fargs, (subexprs[1] or utils.expr("nil", "literal")))
      if (i == len) then
        for j = 2, #subexprs do
          table.insert(fargs, subexprs[j])
        end
      else
        keep_side_effects(subexprs, parent, 2, ast[i])
      end
    end
    local pat
    if ("string" == type(ast[1])) then
      pat = "(%s)(%s)"
    else
      pat = "%s(%s)"
    end
    local call = string.format(pat, tostring(fcallee), exprs1(fargs))
    return handle_compile_opts({utils.expr(call, "statement")}, parent, opts, ast)
  end
  local function compile_call(ast, scope, parent, opts, compile1)
    utils.hook("call", ast, scope)
    local len = #ast
    local first = ast[1]
    local multi_sym_parts = utils["multi-sym?"](first)
    local special = (utils["sym?"](first) and scope.specials[tostring(first)])
    assert_compile((len > 0), "expected a function, macro, or special to call", ast)
    if special then
      return compile_special(ast, scope, parent, opts, special)
    elseif (multi_sym_parts and multi_sym_parts["multi-sym-method-call"]) then
      local table_with_method = table.concat({unpack(multi_sym_parts, 1, (#multi_sym_parts - 1))}, ".")
      local method_to_call = multi_sym_parts[#multi_sym_parts]
      local new_ast = utils.list(utils.sym(":", nil, scope), utils.sym(table_with_method, nil, scope), method_to_call, select(2, unpack(ast)))
      return compile1(new_ast, scope, parent, opts)
    else
      return compile_function_call(ast, scope, parent, opts, compile1, len)
    end
  end
  local function compile_varg(ast, scope, parent, opts)
    assert_compile(scope.vararg, "unexpected vararg", ast)
    return handle_compile_opts({utils.expr("...", "varg")}, parent, opts, ast)
  end
  local function compile_sym(ast, scope, parent, opts)
    local multi_sym_parts = utils["multi-sym?"](ast)
    assert_compile(not (multi_sym_parts and multi_sym_parts["multi-sym-method-call"]), "multisym method calls may only be in call position", ast)
    local e
    if (ast[1] == "nil") then
      e = utils.expr("nil", "literal")
    else
      e = symbol_to_expression(ast, scope, true)
    end
    return handle_compile_opts({e}, parent, opts, ast)
  end
  local function serialize_number(n)
    local _282_ = string.gsub(tostring(n), ",", ".")
    return _282_
  end
  local function compile_scalar(ast, _scope, parent, opts)
    local serialize
    do
      local _283_ = type(ast)
      if (_283_ == "nil") then
        serialize = tostring
      elseif (_283_ == "boolean") then
        serialize = tostring
      elseif (_283_ == "string") then
        serialize = serialize_string
      elseif (_283_ == "number") then
        serialize = serialize_number
      else
        serialize = nil
      end
    end
    return handle_compile_opts({utils.expr(serialize(ast), "literal")}, parent, opts)
  end
  local function compile_table(ast, scope, parent, opts, compile1)
    local buffer = {}
    local function write_other_values(k)
      if ((type(k) ~= "number") or (math.floor(k) ~= k) or (k < 1) or (k > #ast)) then
        if ((type(k) == "string") and utils["valid-lua-identifier?"](k)) then
          return {k, k}
        else
          local _let_285_ = compile1(k, scope, parent, {nval = 1})
          local compiled = _let_285_[1]
          local kstr = ("[" .. tostring(compiled) .. "]")
          return {kstr, k}
        end
      else
        return nil
      end
    end
    do
      local keys
      do
        local tbl_14_auto = {}
        local i_15_auto = #tbl_14_auto
        for k, v in utils.stablepairs(ast) do
          local val_16_auto = write_other_values(k, v)
          if (nil ~= val_16_auto) then
            i_15_auto = (i_15_auto + 1)
            do end (tbl_14_auto)[i_15_auto] = val_16_auto
          else
          end
        end
        keys = tbl_14_auto
      end
      local function _291_(_289_)
        local _arg_290_ = _289_
        local k1 = _arg_290_[1]
        local k2 = _arg_290_[2]
        local _let_292_ = compile1(ast[k2], scope, parent, {nval = 1})
        local v = _let_292_[1]
        return string.format("%s = %s", k1, tostring(v))
      end
      utils.map(keys, _291_, buffer)
    end
    for i = 1, #ast do
      local nval = ((i ~= #ast) and 1)
      table.insert(buffer, exprs1(compile1(ast[i], scope, parent, {nval = nval})))
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
    local _let_294_ = opts0
    local isvar = _let_294_["isvar"]
    local declaration = _let_294_["declaration"]
    local forceglobal = _let_294_["forceglobal"]
    local forceset = _let_294_["forceset"]
    local symtype = _let_294_["symtype"]
    local symtype0 = ("_" .. (symtype or "dst"))
    local setter
    if declaration then
      setter = "local %s = %s"
    else
      setter = "%s = %s"
    end
    local new_manglings = {}
    local function getname(symbol, up1)
      local raw = symbol[1]
      assert_compile(not (opts0.nomulti and utils["multi-sym?"](raw)), ("unexpected multi symbol " .. raw), up1)
      if declaration then
        return declare_local(symbol, nil, scope, symbol, new_manglings)
      else
        local parts = (utils["multi-sym?"](raw) or {raw})
        local meta = scope.symmeta[parts[1]]
        assert_compile(not raw:find(":"), "cannot set method sym", symbol)
        if ((#parts == 1) and not forceset) then
          assert_compile(not (forceglobal and meta), string.format("global %s conflicts with local", tostring(symbol)), symbol)
          assert_compile(not (meta and not meta.var), ("expected var " .. raw), symbol)
          assert_compile((meta or not opts0.noundef), ("expected local " .. parts[1]), symbol)
        else
        end
        if forceglobal then
          assert_compile(not scope.symmeta[scope.unmanglings[raw]], ("global " .. raw .. " conflicts with local"), symbol)
          do end (scope.manglings)[raw] = global_mangling(raw)
          do end (scope.unmanglings)[global_mangling(raw)] = raw
          if allowed_globals then
            table.insert(allowed_globals, raw)
          else
          end
        else
        end
        return symbol_to_expression(symbol, scope)[1]
      end
    end
    local function compile_top_target(lvalues)
      local inits
      local function _300_(_241)
        if scope.manglings[_241] then
          return _241
        else
          return "nil"
        end
      end
      inits = utils.map(lvalues, _300_)
      local init = table.concat(inits, ", ")
      local lvalue = table.concat(lvalues, ", ")
      local plen, plast = #parent, parent[#parent]
      local ret = compile1(from, scope, parent, {target = lvalue})
      if declaration then
        for pi = plen, #parent do
          if (parent[pi] == plast) then
            plen = pi
          else
          end
        end
        if ((#parent == (plen + 1)) and parent[#parent].leaf) then
          parent[#parent]["leaf"] = ("local " .. parent[#parent].leaf)
        elseif (init == "nil") then
          table.insert(parent, (plen + 1), {ast = ast, leaf = ("local " .. lvalue)})
        else
          table.insert(parent, (plen + 1), {ast = ast, leaf = ("local " .. lvalue .. " = " .. init)})
        end
      else
      end
      return ret
    end
    local function destructure_sym(left, rightexprs, up1, top_3f)
      local lname = getname(left, up1)
      check_binding_valid(left, scope, left)
      if top_3f then
        compile_top_target({lname})
      else
        emit(parent, setter:format(lname, exprs1(rightexprs)), left)
      end
      if declaration then
        scope.symmeta[tostring(left)] = {var = isvar}
        return nil
      else
        return nil
      end
    end
    local function destructure_table(left, rightexprs, top_3f, destructure1)
      local s = gensym(scope, symtype0)
      local right
      do
        local _307_
        if top_3f then
          _307_ = exprs1(compile1(from, scope, parent))
        else
          _307_ = exprs1(rightexprs)
        end
        if (_307_ == "") then
          right = "nil"
        elseif (nil ~= _307_) then
          local right0 = _307_
          right = right0
        else
          right = nil
        end
      end
      emit(parent, string.format("local %s = %s", s, right), left)
      for k, v in utils.stablepairs(left) do
        if not (("number" == type(k)) and tostring(left[(k - 1)]):find("^&")) then
          if (utils["sym?"](v) and (tostring(v) == "&")) then
            local unpack_str = "(function (t, k)\n                                      local mt = getmetatable(t)\n                                      if \"table\" == type(mt) and mt.__fennelrest then\n                                         return mt.__fennelrest(t, k)\n                                      else\n                                         return {(table.unpack or unpack)(t, k)}\n                                      end\n                                   end)(%s, %s)"
            local formatted = string.format(string.gsub(unpack_str, "\n%s*", " "), s, k)
            local subexpr = utils.expr(formatted, "expression")
            assert_compile((utils["sequence?"](left) and (nil == left[(k + 2)])), "expected rest argument before last parameter", left)
            destructure1(left[(k + 1)], {subexpr}, left)
          elseif (utils["sym?"](k) and (tostring(k) == "&as")) then
            destructure_sym(v, {utils.expr(tostring(s))}, left)
          elseif (utils["sequence?"](left) and (tostring(v) == "&as")) then
            local _, next_sym, trailing = select(k, unpack(left))
            assert_compile((nil == trailing), "expected &as argument before last parameter", left)
            destructure_sym(next_sym, {utils.expr(tostring(s))}, left)
          else
            local key
            if (type(k) == "string") then
              key = serialize_string(k)
            else
              key = k
            end
            local subexpr = utils.expr(string.format("%s[%s]", s, key), "expression")
            destructure1(v, {subexpr}, left)
          end
        else
        end
      end
      return nil
    end
    local function destructure_values(left, up1, top_3f, destructure1)
      local left_names, tables = {}, {}
      for i, name in ipairs(left) do
        if utils["sym?"](name) then
          table.insert(left_names, getname(name, up1))
        else
          local symname = gensym(scope, symtype0)
          table.insert(left_names, symname)
          do end (tables)[i] = {name, utils.expr(symname, "sym")}
        end
      end
      assert_compile(top_3f, "can't nest multi-value destructuring", left)
      compile_top_target(left_names)
      if declaration then
        for _, sym in ipairs(left) do
          if utils["sym?"](sym) then
            scope.symmeta[tostring(sym)] = {var = isvar}
          else
          end
        end
      else
      end
      for _, pair in utils.stablepairs(tables) do
        destructure1(pair[1], {pair[2]}, left)
      end
      return nil
    end
    local function destructure1(left, rightexprs, up1, top_3f)
      if (utils["sym?"](left) and (left[1] ~= "nil")) then
        destructure_sym(left, rightexprs, up1, top_3f)
      elseif utils["table?"](left) then
        destructure_table(left, rightexprs, top_3f, destructure1)
      elseif utils["list?"](left) then
        destructure_values(left, up1, top_3f, destructure1)
      else
        assert_compile(false, string.format("unable to bind %s %s", type(left), tostring(left)), (((type((up1)[2]) == "table") and (up1)[2]) or up1))
      end
      if top_3f then
        return {returned = true}
      else
        return nil
      end
    end
    local ret = destructure1(to, nil, ast, true)
    utils.hook("destructure", from, to, scope)
    apply_manglings(scope, new_manglings, ast)
    return ret
  end
  local function require_include(ast, scope, parent, opts)
    opts.fallback = function(e)
      utils.warn(("include module not found, falling back to require: %s"):format(tostring(e)))
      return utils.expr(string.format("require(%s)", tostring(e)), "statement")
    end
    return scopes.global.specials.include(ast, scope, parent, opts)
  end
  local function compile_stream(strm, options)
    local opts = utils.copy(options)
    local old_globals = allowed_globals
    local scope = (opts.scope or make_scope(scopes.global))
    local vals = {}
    local chunk = {}
    do end (function(tgt, m, ...) return tgt[m](tgt, ...) end)(utils.root, "set-reset")
    allowed_globals = opts.allowedGlobals
    if (opts.indent == nil) then
      opts.indent = "  "
    else
    end
    if opts.requireAsInclude then
      scope.specials.require = require_include
    else
    end
    utils.root.chunk, utils.root.scope, utils.root.options = chunk, scope, opts
    for _, val in parser.parser(strm, opts.filename, opts) do
      table.insert(vals, val)
    end
    for i = 1, #vals do
      local exprs = compile1(vals[i], scope, chunk, {nval = (((i < #vals) and 0) or nil), tail = (i == #vals)})
      keep_side_effects(exprs, chunk, nil, vals[i])
      if (i == #vals) then
        utils.hook("chunk", vals[i], scope)
      else
      end
    end
    allowed_globals = old_globals
    utils.root.reset()
    return flatten(chunk, opts)
  end
  local function compile_string(str, opts)
    return compile_stream(parser["string-stream"](str), (opts or {}))
  end
  local function compile(ast, opts)
    local opts0 = utils.copy(opts)
    local old_globals = allowed_globals
    local chunk = {}
    local scope = (opts0.scope or make_scope(scopes.global))
    do end (function(tgt, m, ...) return tgt[m](tgt, ...) end)(utils.root, "set-reset")
    allowed_globals = opts0.allowedGlobals
    if (opts0.indent == nil) then
      opts0.indent = "  "
    else
    end
    if opts0.requireAsInclude then
      scope.specials.require = require_include
    else
    end
    utils.root.chunk, utils.root.scope, utils.root.options = chunk, scope, opts0
    local exprs = compile1(ast, scope, chunk, {tail = true})
    keep_side_effects(exprs, chunk, nil, ast)
    utils.hook("chunk", ast, scope)
    allowed_globals = old_globals
    utils.root.reset()
    return flatten(chunk, opts0)
  end
  local function traceback_frame(info)
    if ((info.what == "C") and info.name) then
      return string.format("  [C]: in function '%s'", info.name)
    elseif (info.what == "C") then
      return "  [C]: in ?"
    else
      local remap = sourcemap[info.source]
      if (remap and remap[info.currentline]) then
        if remap[info.currentline][1] then
          info.short_src = sourcemap[("@" .. remap[info.currentline][1])].short_src
        else
          info.short_src = remap.short_src
        end
        info.currentline = (remap[info.currentline][2] or -1)
      else
      end
      if (info.what == "Lua") then
        local function _325_()
          if info.name then
            return ("'" .. info.name .. "'")
          else
            return "?"
          end
        end
        return string.format("  %s:%d: in function %s", info.short_src, info.currentline, _325_())
      elseif (info.short_src == "(tail call)") then
        return "  (tail call)"
      else
        return string.format("  %s:%d: in main chunk", info.short_src, info.currentline)
      end
    end
  end
  local function traceback(msg, start)
    local msg0 = tostring((msg or ""))
    if ((msg0:find("^Compile error") or msg0:find("^Parse error")) and not utils["debug-on?"]("trace")) then
      return msg0
    else
      local lines = {}
      if (msg0:find(":%d+: Compile error") or msg0:find(":%d+: Parse error")) then
        table.insert(lines, msg0)
      else
        local newmsg = msg0:gsub("^[^:]*:%d+:%s+", "runtime error: ")
        table.insert(lines, newmsg)
      end
      table.insert(lines, "stack traceback:")
      local done_3f, level = false, (start or 2)
      while not done_3f do
        do
          local _329_ = debug.getinfo(level, "Sln")
          if (_329_ == nil) then
            done_3f = true
          elseif (nil ~= _329_) then
            local info = _329_
            table.insert(lines, traceback_frame(info))
          else
          end
        end
        level = (level + 1)
      end
      return table.concat(lines, "\n")
    end
  end
  local function entry_transform(fk, fv)
    local function _332_(k, v)
      if (type(k) == "number") then
        return k, fv(v)
      else
        return fk(k), fv(v)
      end
    end
    return _332_
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
      else
      end
    end
    return ret
  end
  local function do_quote(form, scope, parent, runtime_3f)
    local function q(x)
      return do_quote(x, scope, parent, runtime_3f)
    end
    if utils["varg?"](form) then
      assert_compile(not runtime_3f, "quoted ... may only be used at compile time", form)
      return "_VARARG"
    elseif utils["sym?"](form) then
      local filename
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
    elseif (utils["list?"](form) and utils["sym?"](form[1]) and (tostring(form[1]) == "unquote")) then
      local payload = form[2]
      local res = unpack(compile1(payload, scope, parent))
      return res[1]
    elseif utils["list?"](form) then
      local mapped
      local function _337_()
        return nil
      end
      mapped = utils.kvmap(form, entry_transform(_337_, q))
      local filename
      if form.filename then
        filename = string.format("%q", form.filename)
      else
        filename = "nil"
      end
      assert_compile(not runtime_3f, "lists may only be used at compile time", form)
      return string.format(("setmetatable({filename=%s, line=%s, bytestart=%s, %s}" .. ", getmetatable(list()))"), filename, (form.line or "nil"), (form.bytestart or "nil"), mixed_concat(mapped, ", "))
    elseif utils["sequence?"](form) then
      local mapped = utils.kvmap(form, entry_transform(q, q))
      local source = getmetatable(form)
      local filename
      if source.filename then
        filename = string.format("%q", source.filename)
      else
        filename = "nil"
      end
      local _340_
      if source then
        _340_ = source.line
      else
        _340_ = "nil"
      end
      return string.format("setmetatable({%s}, {filename=%s, line=%s, sequence=%s})", mixed_concat(mapped, ", "), filename, _340_, "(getmetatable(sequence()))['sequence']")
    elseif (type(form) == "table") then
      local mapped = utils.kvmap(form, entry_transform(q, q))
      local source = getmetatable(form)
      local filename
      if source.filename then
        filename = string.format("%q", source.filename)
      else
        filename = "nil"
      end
      local function _343_()
        if source then
          return source.line
        else
          return "nil"
        end
      end
      return string.format("setmetatable({%s}, {filename=%s, line=%s})", mixed_concat(mapped, ", "), filename, _343_())
    elseif (type(form) == "string") then
      return serialize_string(form)
    else
      return tostring(form)
    end
  end
  return {compile = compile, compile1 = compile1, ["compile-stream"] = compile_stream, ["compile-string"] = compile_string, emit = emit, destructure = destructure, ["require-include"] = require_include, autogensym = autogensym, gensym = gensym, ["do-quote"] = do_quote, ["global-mangling"] = global_mangling, ["global-unmangling"] = global_unmangling, ["apply-manglings"] = apply_manglings, macroexpand = macroexpand_2a, ["declare-local"] = declare_local, ["make-scope"] = make_scope, ["keep-side-effects"] = keep_side_effects, ["symbol-to-expression"] = symbol_to_expression, assert = assert_compile, scopes = scopes, traceback = traceback, metadata = make_metadata(), sourcemap = sourcemap}
end
package.preload["fennel.friend"] = package.preload["fennel.friend"] or function(...)
  local utils = require("fennel.utils")
  local suggestions = {["unexpected multi symbol (.*)"] = {"removing periods or colons from %s"}, ["use of global (.*) is aliased by a local"] = {"renaming local %s", "refer to the global using _G.%s instead of directly"}, ["local (.*) was overshadowed by a special form or macro"] = {"renaming local %s"}, ["global (.*) conflicts with local"] = {"renaming local %s"}, ["expected var (.*)"] = {"declaring %s using var instead of let/local", "introducing a new local instead of changing the value of %s"}, ["expected macros to be table"] = {"ensuring your macro definitions return a table"}, ["expected each macro to be function"] = {"ensuring that the value for each key in your macros table contains a function", "avoid defining nested macro tables"}, ["macro not found in macro module"] = {"checking the keys of the imported macro module's returned table"}, ["macro tried to bind (.*) without gensym"] = {"changing to %s# when introducing identifiers inside macros"}, ["unknown identifier in strict mode: (.*)"] = {"looking to see if there's a typo", "using the _G table instead, eg. _G.%s if you really want a global", "moving this code to somewhere that %s is in scope", "binding %s as a local in the scope of this code"}, ["expected a function.* to call"] = {"removing the empty parentheses", "using square brackets if you want an empty table"}, ["cannot call literal value"] = {"checking for typos", "checking for a missing function name"}, ["unexpected vararg"] = {"putting \"...\" at the end of the fn parameters if the vararg was intended"}, ["multisym method calls may only be in call position"] = {"using a period instead of a colon to reference a table's fields", "putting parens around this"}, ["unused local (.*)"] = {"renaming the local to _%s if it is meant to be unused", "fixing a typo so %s is used", "disabling the linter which checks for unused locals"}, ["expected parameters"] = {"adding function parameters as a list of identifiers in brackets"}, ["unable to bind (.*)"] = {"replacing the %s with an identifier"}, ["expected rest argument before last parameter"] = {"moving & to right before the final identifier when destructuring"}, ["expected vararg as last parameter"] = {"moving the \"...\" to the end of the parameter list"}, ["expected symbol for function parameter: (.*)"] = {"changing %s to an identifier instead of a literal value"}, ["could not compile value of type "] = {"debugging the macro you're calling to return a list or table"}, ["expected local"] = {"looking for a typo", "looking for a local which is used out of its scope"}, ["expected body expression"] = {"putting some code in the body of this form after the bindings"}, ["expected binding and iterator"] = {"making sure you haven't omitted a local name or iterator"}, ["expected binding sequence"] = {"placing a table here in square brackets containing identifiers to bind"}, ["expected even number of name/value bindings"] = {"finding where the identifier or value is missing"}, ["may only be used at compile time"] = {"moving this to inside a macro if you need to manipulate symbols/lists", "using square brackets instead of parens to construct a table"}, ["unexpected closing delimiter (.)"] = {"deleting %s", "adding matching opening delimiter earlier"}, ["mismatched closing delimiter (.), expected (.)"] = {"replacing %s with %s", "deleting %s", "adding matching opening delimiter earlier"}, ["expected even number of values in table literal"] = {"removing a key", "adding a value"}, ["expected whitespace before opening delimiter"] = {"adding whitespace"}, ["illegal character: (.)"] = {"deleting or replacing %s", "avoiding reserved characters like \", \\, ', ~, ;, @, `, and comma"}, ["could not read number (.*)"] = {"removing the non-digit character", "beginning the identifier with a non-digit if it is not meant to be a number"}, ["can't start multisym segment with a digit"] = {"removing the digit", "adding a non-digit before the digit"}, ["malformed multisym"] = {"ensuring each period or colon is not followed by another period or colon"}, ["method must be last component"] = {"using a period instead of a colon for field access", "removing segments after the colon", "making the method call, then looking up the field on the result"}, ["$ and $... in hashfn are mutually exclusive"] = {"modifying the hashfn so it only contains $... or $, $1, $2, $3, etc"}, ["tried to reference a macro at runtime"] = {"renaming the macro so as not to conflict with locals"}, ["expected even number of pattern/body pairs"] = {"checking that every pattern has a body to go with it", "adding _ before the final body"}, ["unexpected arguments"] = {"removing an argument", "checking for typos"}, ["unexpected iterator clause"] = {"removing an argument", "checking for typos"}}
  local unpack = (table.unpack or _G.unpack)
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
      else
      end
    end
    return suggestion
  end
  local function read_line_from_file(filename, line)
    local bytes = 0
    local f = assert(io.open(filename))
    local _
    for _0 = 1, (line - 1) do
      bytes = (bytes + 1 + #f:read())
    end
    _ = nil
    local codeline = f:read()
    f:close()
    return codeline, bytes
  end
  local function read_line_from_string(matcher, target_line, _3fcurrent_line, _3fbytes)
    local this_line, newline = matcher()
    local current_line = (_3fcurrent_line or 1)
    local bytes = ((_3fbytes or 0) + #this_line + #newline)
    if (target_line == current_line) then
      return this_line, (bytes - #this_line - 1)
    elseif this_line then
      return read_line_from_string(matcher, target_line, (current_line + 1), bytes)
    else
      return nil
    end
  end
  local function read_line(filename, line, source)
    if source then
      return read_line_from_string(string.gmatch((source .. "\n"), "(.-)(\13?\n)"), line)
    else
      return read_line_from_file(filename, line)
    end
  end
  local function friendly_msg(msg, _142_, source)
    local _arg_143_ = _142_
    local filename = _arg_143_["filename"]
    local line = _arg_143_["line"]
    local bytestart = _arg_143_["bytestart"]
    local byteend = _arg_143_["byteend"]
    local ok, codeline, bol = pcall(read_line, filename, line, source)
    local suggestions0 = suggest(msg)
    local out = {msg, ""}
    if (ok and codeline) then
      table.insert(out, codeline)
    else
    end
    if (ok and codeline and bytestart and byteend) then
      table.insert(out, (string.rep(" ", (bytestart - bol - 1)) .. "^" .. string.rep("^", math.min((byteend - bytestart), ((bol + #codeline) - bytestart)))))
    else
    end
    if (ok and codeline and bytestart and not byteend) then
      table.insert(out, (string.rep("-", (bytestart - bol - 1)) .. "^"))
      table.insert(out, "")
    else
    end
    if suggestions0 then
      for _, suggestion in ipairs(suggestions0) do
        table.insert(out, ("* Try %s."):format(suggestion))
      end
    else
    end
    return table.concat(out, "\n")
  end
  local function assert_compile(condition, msg, ast, source)
    if not condition then
      local _let_148_ = utils["ast-source"](ast)
      local filename = _let_148_["filename"]
      local line = _let_148_["line"]
      error(friendly_msg(("Compile error in %s:%s\n  %s"):format((filename or "unknown"), (line or "?"), msg), utils["ast-source"](ast), source), 0)
    else
    end
    return condition
  end
  local function parse_error(msg, filename, line, bytestart, source)
    return error(friendly_msg(("Parse error in %s:%s\n  %s"):format(filename, line, msg), {filename = filename, line = line, bytestart = bytestart}, source), 0)
  end
  return {["assert-compile"] = assert_compile, ["parse-error"] = parse_error}
end
package.preload["fennel.parser"] = package.preload["fennel.parser"] or function(...)
  local utils = require("fennel.utils")
  local friend = require("fennel.friend")
  local unpack = (table.unpack or _G.unpack)
  local function granulate(getchunk)
    local c, index, done_3f = "", 1, false
    local function _150_(parser_state)
      if not done_3f then
        if (index <= #c) then
          local b = c:byte(index)
          index = (index + 1)
          return b
        else
          local _151_ = getchunk(parser_state)
          local function _152_()
            local char = _151_
            return (char ~= "")
          end
          if ((nil ~= _151_) and _152_()) then
            local char = _151_
            c = char
            index = 2
            return c:byte()
          elseif true then
            local _ = _151_
            done_3f = true
            return nil
          else
            return nil
          end
        end
      else
        return nil
      end
    end
    local function _156_()
      c = ""
      return nil
    end
    return _150_, _156_
  end
  local function string_stream(str)
    local str0 = str:gsub("^#!", ";;")
    local index = 1
    local function _157_()
      local r = str0:byte(index)
      index = (index + 1)
      return r
    end
    return _157_
  end
  local delims = {[40] = 41, [41] = true, [91] = 93, [93] = true, [123] = 125, [125] = true}
  local function whitespace_3f(b)
    return ((b == 32) or ((b >= 9) and (b <= 13)))
  end
  local function sym_char_3f(b)
    local b0
    if ("number" == type(b)) then
      b0 = b
    else
      b0 = string.byte(b)
    end
    return ((b0 > 32) and not delims[b0] and (b0 ~= 127) and (b0 ~= 34) and (b0 ~= 39) and (b0 ~= 126) and (b0 ~= 59) and (b0 ~= 44) and (b0 ~= 64) and (b0 ~= 96))
  end
  local prefixes = {[35] = "hashfn", [39] = "quote", [44] = "unquote", [96] = "quote"}
  local function parser(getbyte, _3ffilename, _3foptions)
    local stack = {}
    local line = 1
    local byteindex = 0
    local lastb = nil
    local function ungetb(ub)
      if (ub == 10) then
        line = (line - 1)
      else
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
      byteindex = (byteindex + 1)
      if (r == 10) then
        line = (line + 1)
      else
      end
      return r
    end
    assert(((nil == _3ffilename) or ("string" == type(_3ffilename))), "expected filename as second argument to parser")
    local function parse_error(msg, byteindex_override)
      local _let_162_ = (_3foptions or utils.root.options or {})
      local source = _let_162_["source"]
      local unfriendly = _let_162_["unfriendly"]
      if (nil == utils.hook("parse-error", msg, (_3ffilename or "unknown"), (line or "?"), (byteindex_override or byteindex), source, utils.root.reset)) then
        utils.root.reset()
        if (unfriendly or not friend or not _G.io or not _G.io.read) then
          return error(string.format("%s:%s: Parse error: %s", (_3ffilename or "unknown"), (line or "?"), msg), 0)
        else
          return friend["parse-error"](msg, (_3ffilename or "unknown"), (line or "?"), (byteindex_override or byteindex), source)
        end
      else
        return nil
      end
    end
    local function parse_stream()
      local whitespace_since_dispatch, done_3f, retval = true
      local function dispatch(v)
        local _165_ = stack[#stack]
        if (_165_ == nil) then
          retval, done_3f, whitespace_since_dispatch = v, true, false
          return nil
        elseif ((_G.type(_165_) == "table") and (nil ~= (_165_).prefix)) then
          local prefix = (_165_).prefix
          local source
          do
            local _166_ = table.remove(stack)
            do end (_166_)["byteend"] = byteindex
            source = _166_
          end
          local list = utils.list(utils.sym(prefix, source), v)
          for k, v0 in pairs(source) do
            list[k] = v0
          end
          return dispatch(list)
        elseif (nil ~= _165_) then
          local top = _165_
          whitespace_since_dispatch = false
          return table.insert(top, v)
        else
          return nil
        end
      end
      local function badend()
        local accum = utils.map(stack, "closer")
        local _168_
        if (#stack == 1) then
          _168_ = ""
        else
          _168_ = "s"
        end
        return parse_error(string.format("expected closing delimiter%s %s", _168_, string.char(unpack(accum))))
      end
      local function skip_whitespace(b)
        if (b and whitespace_3f(b)) then
          whitespace_since_dispatch = true
          return skip_whitespace(getb())
        elseif (not b and (#stack > 0)) then
          return badend()
        else
          return b
        end
      end
      local function parse_comment(b, contents)
        if (b and (10 ~= b)) then
          local function _172_()
            local _171_ = contents
            table.insert(_171_, string.char(b))
            return _171_
          end
          return parse_comment(getb(), _172_())
        elseif (_3foptions and _3foptions.comments) then
          return dispatch(utils.comment(table.concat(contents), {line = (line - 1), filename = _3ffilename}))
        else
          return b
        end
      end
      local function open_table(b)
        if not whitespace_since_dispatch then
          parse_error(("expected whitespace before opening delimiter " .. string.char(b)))
        else
        end
        return table.insert(stack, {bytestart = byteindex, closer = delims[b], filename = _3ffilename, line = line})
      end
      local function close_list(list)
        return dispatch(setmetatable(list, getmetatable(utils.list())))
      end
      local function close_sequence(tbl)
        local val = utils.sequence(unpack(tbl))
        for k, v in pairs(tbl) do
          getmetatable(val)[k] = v
        end
        return dispatch(val)
      end
      local function add_comment_at(comments, index, node)
        local _175_ = comments[index]
        if (nil ~= _175_) then
          local existing = _175_
          return table.insert(existing, node)
        elseif true then
          local _ = _175_
          comments[index] = {node}
          return nil
        else
          return nil
        end
      end
      local function next_noncomment(tbl, i)
        if utils["comment?"](tbl[i]) then
          return next_noncomment(tbl, (i + 1))
        else
          return tbl[i]
        end
      end
      local function extract_comments(tbl)
        local comments = {keys = {}, values = {}, last = {}}
        while utils["comment?"](tbl[#tbl]) do
          table.insert(comments.last, 1, table.remove(tbl))
        end
        local last_key_3f = false
        for i, node in ipairs(tbl) do
          if not utils["comment?"](node) then
            last_key_3f = not last_key_3f
          elseif last_key_3f then
            add_comment_at(comments.values, next_noncomment(tbl, i), node)
          else
            add_comment_at(comments.keys, next_noncomment(tbl, i), node)
          end
        end
        for i = #tbl, 1, -1 do
          if utils["comment?"](tbl[i]) then
            table.remove(tbl, i)
          else
          end
        end
        return comments
      end
      local function close_curly_table(tbl)
        local comments = extract_comments(tbl)
        local keys = {}
        local val = {}
        if ((#tbl % 2) ~= 0) then
          byteindex = (byteindex - 1)
          parse_error("expected even number of values in table literal")
        else
        end
        setmetatable(val, tbl)
        for i = 1, #tbl, 2 do
          if ((tostring(tbl[i]) == ":") and utils["sym?"](tbl[(i + 1)]) and utils["sym?"](tbl[i])) then
            tbl[i] = tostring(tbl[(i + 1)])
          else
          end
          val[tbl[i]] = tbl[(i + 1)]
          table.insert(keys, tbl[i])
        end
        tbl.comments = comments
        tbl.keys = keys
        return dispatch(val)
      end
      local function close_table(b)
        local top = table.remove(stack)
        if (top == nil) then
          parse_error(("unexpected closing delimiter " .. string.char(b)))
        else
        end
        if (top.closer and (top.closer ~= b)) then
          parse_error(("mismatched closing delimiter " .. string.char(b) .. ", expected " .. string.char(top.closer)))
        else
        end
        top.byteend = byteindex
        if (b == 41) then
          return close_list(top)
        elseif (b == 93) then
          return close_sequence(top)
        else
          return close_curly_table(top)
        end
      end
      local function parse_string_loop(chars, b, state)
        table.insert(chars, b)
        local state0
        do
          local _185_ = {state, b}
          if ((_G.type(_185_) == "table") and ((_185_)[1] == "base") and ((_185_)[2] == 92)) then
            state0 = "backslash"
          elseif ((_G.type(_185_) == "table") and ((_185_)[1] == "base") and ((_185_)[2] == 34)) then
            state0 = "done"
          elseif ((_G.type(_185_) == "table") and ((_185_)[1] == "backslash") and ((_185_)[2] == 10)) then
            table.remove(chars, (#chars - 1))
            state0 = "base"
          elseif true then
            local _ = _185_
            state0 = "base"
          else
            state0 = nil
          end
        end
        if (b and (state0 ~= "done")) then
          return parse_string_loop(chars, getb(), state0)
        else
          return b
        end
      end
      local function escape_char(c)
        return ({[7] = "\\a", [8] = "\\b", [9] = "\\t", [10] = "\\n", [11] = "\\v", [12] = "\\f", [13] = "\\r"})[c:byte()]
      end
      local function parse_string()
        table.insert(stack, {closer = 34})
        local chars = {34}
        if not parse_string_loop(chars, getb(), "base") then
          badend()
        else
        end
        table.remove(stack)
        local raw = string.char(unpack(chars))
        local formatted = raw:gsub("[\7-\13]", escape_char)
        local _189_ = (rawget(_G, "loadstring") or load)(("return " .. formatted))
        if (nil ~= _189_) then
          local load_fn = _189_
          return dispatch(load_fn())
        elseif (_189_ == nil) then
          return parse_error(("Invalid string: " .. raw))
        else
          return nil
        end
      end
      local function parse_prefix(b)
        table.insert(stack, {prefix = prefixes[b], filename = _3ffilename, line = line, bytestart = byteindex})
        local nextb = getb()
        if (whitespace_3f(nextb) or (true == delims[nextb])) then
          if (b ~= 35) then
            parse_error("invalid whitespace after quoting prefix")
          else
          end
          table.remove(stack)
          dispatch(utils.sym("#"))
        else
        end
        return ungetb(nextb)
      end
      local function parse_sym_loop(chars, b)
        if (b and sym_char_3f(b)) then
          table.insert(chars, b)
          return parse_sym_loop(chars, getb())
        else
          if b then
            ungetb(b)
          else
          end
          return chars
        end
      end
      local function parse_number(rawstr)
        local number_with_stripped_underscores = (not rawstr:find("^_") and rawstr:gsub("_", ""))
        if rawstr:match("^%d") then
          dispatch((tonumber(number_with_stripped_underscores) or parse_error(("could not read number \"" .. rawstr .. "\""))))
          return true
        else
          local _195_ = tonumber(number_with_stripped_underscores)
          if (nil ~= _195_) then
            local x = _195_
            dispatch(x)
            return true
          elseif true then
            local _ = _195_
            return false
          else
            return nil
          end
        end
      end
      local function check_malformed_sym(rawstr)
        if (rawstr:match("^~") and (rawstr ~= "~=")) then
          return parse_error("illegal character: ~")
        elseif rawstr:match("%.[0-9]") then
          return parse_error(("can't start multisym segment with a digit: " .. rawstr), (((byteindex - #rawstr) + rawstr:find("%.[0-9]")) + 1))
        elseif (rawstr:match("[%.:][%.:]") and (rawstr ~= "..") and (rawstr ~= "$...")) then
          return parse_error(("malformed multisym: " .. rawstr), ((byteindex - #rawstr) + 1 + rawstr:find("[%.:][%.:]")))
        elseif ((rawstr ~= ":") and rawstr:match(":$")) then
          return parse_error(("malformed multisym: " .. rawstr), ((byteindex - #rawstr) + 1 + rawstr:find(":$")))
        elseif rawstr:match(":.+[%.:]") then
          return parse_error(("method must be last component of multisym: " .. rawstr), ((byteindex - #rawstr) + rawstr:find(":.+[%.:]")))
        else
          return rawstr
        end
      end
      local function parse_sym(b)
        local bytestart = byteindex
        local rawstr = string.char(unpack(parse_sym_loop({b}, getb())))
        if (rawstr == "true") then
          return dispatch(true)
        elseif (rawstr == "false") then
          return dispatch(false)
        elseif (rawstr == "...") then
          return dispatch(utils.varg())
        elseif rawstr:match("^:.+$") then
          return dispatch(rawstr:sub(2))
        elseif not parse_number(rawstr) then
          return dispatch(utils.sym(check_malformed_sym(rawstr), {byteend = byteindex, bytestart = bytestart, filename = _3ffilename, line = line}))
        else
          return nil
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
          parse_string(b)
        elseif prefixes[b] then
          parse_prefix(b)
        elseif (sym_char_3f(b) or (b == string.byte("~"))) then
          parse_sym(b)
        elseif not utils.hook("illegal-char", b, getb, ungetb, dispatch) then
          parse_error(("illegal character: " .. string.char(b)))
        else
        end
        if not b then
          return nil
        elseif done_3f then
          return true, retval
        else
          return parse_loop(skip_whitespace(getb()))
        end
      end
      return parse_loop(skip_whitespace(getb()))
    end
    local function _202_()
      stack, line, byteindex, lastb = {}, 1, 0, nil
      return nil
    end
    return parse_stream, _202_
  end
  return {granulate = granulate, parser = parser, ["string-stream"] = string_stream, ["sym-char?"] = sym_char_3f}
end
local utils
package.preload["fennel.view"] = package.preload["fennel.view"] or function(...)
  local type_order = {number = 1, boolean = 2, string = 3, table = 4, ["function"] = 5, userdata = 6, thread = 7}
  local lua_pairs = pairs
  local lua_ipairs = ipairs
  local function pairs(t)
    local _1_ = getmetatable(t)
    if ((_G.type(_1_) == "table") and (nil ~= (_1_).__pairs)) then
      local p = (_1_).__pairs
      return p(t)
    elseif true then
      local _ = _1_
      return lua_pairs(t)
    else
      return nil
    end
  end
  local function ipairs(t)
    local _3_ = getmetatable(t)
    if ((_G.type(_3_) == "table") and (nil ~= (_3_).__ipairs)) then
      local i = (_3_).__ipairs
      return i(t)
    elseif true then
      local _ = _3_
      return lua_ipairs(t)
    else
      return nil
    end
  end
  local function length_2a(t)
    local _5_ = getmetatable(t)
    if ((_G.type(_5_) == "table") and (nil ~= (_5_).__len)) then
      local l = (_5_).__len
      return l(t)
    elseif true then
      local _ = _5_
      return #t
    else
      return nil
    end
  end
  local function sort_keys(_7_, _9_)
    local _arg_8_ = _7_
    local a = _arg_8_[1]
    local _arg_10_ = _9_
    local b = _arg_10_[1]
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
    if (length_2a(kv) > 0) then
      local i = 0
      for _, _13_ in ipairs(kv) do
        local _each_14_ = _13_
        local k = _each_14_[1]
        if ((k - i) > gap) then
          gap = (k - i)
        else
        end
        i = k
      end
    else
    end
    return gap
  end
  local function fill_gaps(kv)
    local missing_indexes = {}
    local i = 0
    for _, _17_ in ipairs(kv) do
      local _each_18_ = _17_
      local j = _each_18_[1]
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
      else
      end
      insert(kv, {k, v})
    end
    table.sort(kv, sort_keys)
    if not assoc_3f then
      if (max_index_gap(kv) > options["max-sparse-gap"]) then
        assoc_3f = true
      else
        fill_gaps(kv)
      end
    else
    end
    if (length_2a(kv) == 0) then
      return kv, "empty"
    else
      local function _22_()
        if assoc_3f then
          return "table"
        else
          return "seq"
        end
      end
      return kv, _22_()
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
    else
    end
    return appearances
  end
  local function save_table(t, seen)
    local seen0 = (seen or {len = 0})
    local id = (seen0.len + 1)
    if not (seen0)[t] then
      seen0[t] = id
      seen0.len = id
    else
    end
    return seen0
  end
  local function detect_cycle(t, seen, _3fk)
    if ("table" == type(t)) then
      seen[t] = true
      local _27_, _28_ = next(t, _3fk)
      if ((nil ~= _27_) and (nil ~= _28_)) then
        local k = _27_
        local v = _28_
        return (seen[k] or detect_cycle(k, seen) or seen[v] or detect_cycle(v, seen) or detect_cycle(t, seen, k))
      else
        return nil
      end
    else
      return nil
    end
  end
  local function visible_cycle_3f(t, options)
    return (options["detect-cycles?"] and detect_cycle(t, {}) and save_table(t, options.seen) and (1 < (options.appearances[t] or 0)))
  end
  local function table_indent(indent, id)
    local opener_length
    if id then
      opener_length = (length_2a(tostring(id)) + 2)
    else
      opener_length = 1
    end
    return (indent + opener_length)
  end
  local pp = nil
  local function concat_table_lines(elements, options, multiline_3f, indent, table_type, prefix)
    local indent_str = ("\n" .. string.rep(" ", indent))
    local open
    local function _32_()
      if ("seq" == table_type) then
        return "["
      else
        return "{"
      end
    end
    open = ((prefix or "") .. _32_())
    local close
    if ("seq" == table_type) then
      close = "]"
    else
      close = "}"
    end
    local oneline = (open .. table.concat(elements, " ") .. close)
    if (not options["one-line?"] and (multiline_3f or ((indent + length_2a(oneline)) > options["line-length"]))) then
      return (open .. table.concat(elements, indent_str) .. close)
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
  local function pp_associative(t, kv, options, indent)
    local multiline_3f = false
    local id = options.seen[t]
    if (options.level >= options.depth) then
      return "{...}"
    elseif (id and options["detect-cycles?"]) then
      return ("@" .. id .. "{...}")
    else
      local visible_cycle_3f0 = visible_cycle_3f(t, options)
      local id0 = (visible_cycle_3f0 and options.seen[t])
      local indent0 = table_indent(indent, id0)
      local slength
      if options["utf8?"] then
        slength = utf8_len
      else
        local function _35_(_241)
          return #_241
        end
        slength = _35_
      end
      local prefix
      if visible_cycle_3f0 then
        prefix = ("@" .. id0)
      else
        prefix = ""
      end
      local items
      do
        local tbl_14_auto = {}
        local i_15_auto = #tbl_14_auto
        for _, _38_ in pairs(kv) do
          local _each_39_ = _38_
          local k = _each_39_[1]
          local v = _each_39_[2]
          local val_16_auto
          do
            local k0 = pp(k, options, (indent0 + 1), true)
            local v0 = pp(v, options, (indent0 + slength(k0) + 1))
            multiline_3f = (multiline_3f or k0:find("\n") or v0:find("\n"))
            val_16_auto = (k0 .. " " .. v0)
          end
          if (nil ~= val_16_auto) then
            i_15_auto = (i_15_auto + 1)
            do end (tbl_14_auto)[i_15_auto] = val_16_auto
          else
          end
        end
        items = tbl_14_auto
      end
      return concat_table_lines(items, options, multiline_3f, indent0, "table", prefix)
    end
  end
  local function pp_sequence(t, kv, options, indent)
    local multiline_3f = false
    local id = options.seen[t]
    if (options.level >= options.depth) then
      return "[...]"
    elseif (id and options["detect-cycles?"]) then
      return ("@" .. id .. "[...]")
    else
      local visible_cycle_3f0 = visible_cycle_3f(t, options)
      local id0 = (visible_cycle_3f0 and options.seen[t])
      local indent0 = table_indent(indent, id0)
      local prefix
      if visible_cycle_3f0 then
        prefix = ("@" .. id0)
      else
        prefix = ""
      end
      local items
      do
        local tbl_14_auto = {}
        local i_15_auto = #tbl_14_auto
        for _, _43_ in pairs(kv) do
          local _each_44_ = _43_
          local _0 = _each_44_[1]
          local v = _each_44_[2]
          local val_16_auto
          do
            local v0 = pp(v, options, indent0)
            multiline_3f = (multiline_3f or v0:find("\n"))
            val_16_auto = v0
          end
          if (nil ~= val_16_auto) then
            i_15_auto = (i_15_auto + 1)
            do end (tbl_14_auto)[i_15_auto] = val_16_auto
          else
          end
        end
        items = tbl_14_auto
      end
      return concat_table_lines(items, options, multiline_3f, indent0, "seq", prefix)
    end
  end
  local function concat_lines(lines, options, indent, force_multi_line_3f)
    if (length_2a(lines) == 0) then
      if options["empty-as-sequence?"] then
        return "[]"
      else
        return "{}"
      end
    else
      local oneline
      local _48_
      do
        local tbl_14_auto = {}
        local i_15_auto = #tbl_14_auto
        for _, line in ipairs(lines) do
          local val_16_auto = line:gsub("^%s+", "")
          if (nil ~= val_16_auto) then
            i_15_auto = (i_15_auto + 1)
            do end (tbl_14_auto)[i_15_auto] = val_16_auto
          else
          end
        end
        _48_ = tbl_14_auto
      end
      oneline = table.concat(_48_, " ")
      if (not options["one-line?"] and (force_multi_line_3f or oneline:find("\n") or ((indent + length_2a(oneline)) > options["line-length"]))) then
        return table.concat(lines, ("\n" .. string.rep(" ", indent)))
      else
        return oneline
      end
    end
  end
  local function pp_metamethod(t, metamethod, options, indent)
    if (options.level >= options.depth) then
      if options["empty-as-sequence?"] then
        return "[...]"
      else
        return "{...}"
      end
    else
      local _
      local function _53_(_241)
        return visible_cycle_3f(_241, options)
      end
      options["visible-cycle?"] = _53_
      _ = nil
      local lines, force_multi_line_3f = metamethod(t, pp, options, indent)
      options["visible-cycle?"] = nil
      local _54_ = type(lines)
      if (_54_ == "string") then
        return lines
      elseif (_54_ == "table") then
        return concat_lines(lines, options, indent, force_multi_line_3f)
      elseif true then
        local _0 = _54_
        return error("__fennelview metamethod must return a table of lines")
      else
        return nil
      end
    end
  end
  local function pp_table(x, options, indent)
    options.level = (options.level + 1)
    local x0
    do
      local _57_
      if options["metamethod?"] then
        local _58_ = x
        if (nil ~= _58_) then
          local _59_ = getmetatable(_58_)
          if (nil ~= _59_) then
            _57_ = (_59_).__fennelview
          else
            _57_ = _59_
          end
        else
          _57_ = _58_
        end
      else
        _57_ = nil
      end
      if (nil ~= _57_) then
        local metamethod = _57_
        x0 = pp_metamethod(x, metamethod, options, indent)
      elseif true then
        local _ = _57_
        local _63_, _64_ = table_kv_pairs(x, options)
        if (true and (_64_ == "empty")) then
          local _0 = _63_
          if options["empty-as-sequence?"] then
            x0 = "[]"
          else
            x0 = "{}"
          end
        elseif ((nil ~= _63_) and (_64_ == "table")) then
          local kv = _63_
          x0 = pp_associative(x, kv, options, indent)
        elseif ((nil ~= _63_) and (_64_ == "seq")) then
          local kv = _63_
          x0 = pp_sequence(x, kv, options, indent)
        else
          x0 = nil
        end
      else
        x0 = nil
      end
    end
    options.level = (options.level - 1)
    return x0
  end
  local function number__3estring(n)
    local _68_ = string.gsub(tostring(n), ",", ".")
    return _68_
  end
  local function colon_string_3f(s)
    return s:find("^[-%w?^_!$%&*+./@|<=>]+$")
  end
  local utf8_inits = {{["min-byte"] = 0, ["max-byte"] = 127, ["min-code"] = 0, ["max-code"] = 127, len = 1}, {["min-byte"] = 192, ["max-byte"] = 223, ["min-code"] = 128, ["max-code"] = 2047, len = 2}, {["min-byte"] = 224, ["max-byte"] = 239, ["min-code"] = 2048, ["max-code"] = 65535, len = 3}, {["min-byte"] = 240, ["max-byte"] = 247, ["min-code"] = 65536, ["max-code"] = 1114111, len = 4}}
  local function utf8_escape(str)
    local function validate_utf8(str0, index)
      local inits = utf8_inits
      local byte = string.byte(str0, index)
      local init
      do
        local ret = nil
        for _, init0 in ipairs(inits) do
          if ret then break end
          ret = (byte and (function(_69_,_70_,_71_) return (_69_ >= _70_) and (_70_ >= _71_) end)(init0["max-byte"],byte,init0["min-byte"]) and init0)
        end
        init = ret
      end
      local code
      local function _72_()
        local code0
        if init then
          code0 = (byte - init["min-byte"])
        else
          code0 = nil
        end
        for i = (index + 1), (index + init.len + -1) do
          local byte0 = string.byte(str0, i)
          code0 = (byte0 and code0 and (function(_74_,_75_,_76_) return (_74_ >= _75_) and (_75_ >= _76_) end)(191,byte0,128) and ((code0 * 64) + (byte0 - 128)))
        end
        return code0
      end
      code = (init and _72_())
      if (code and (function(_77_,_78_,_79_) return (_77_ >= _78_) and (_78_ >= _79_) end)(init["max-code"],code,init["min-code"]) and not (function(_80_,_81_,_82_) return (_80_ >= _81_) and (_81_ >= _82_) end)(57343,code,55296)) then
        return init.len
      else
        return nil
      end
    end
    local index = 1
    local output = {}
    while (index <= #str) do
      local nexti = (string.find(str, "[\128-\255]", index) or (#str + 1))
      local len = validate_utf8(str, nexti)
      table.insert(output, string.sub(str, index, (nexti + (len or 0) + -1)))
      if (not len and (nexti <= #str)) then
        table.insert(output, string.format("\\%03d", string.byte(str, nexti)))
      else
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
    local escs
    local _86_
    if (options["escape-newlines?"] and (length_2a(str) < (options["line-length"] - indent))) then
      _86_ = "\\n"
    else
      _86_ = "\n"
    end
    local function _88_(_241, _242)
      return ("\\%03d"):format(_242:byte())
    end
    escs = setmetatable({["\7"] = "\\a", ["\8"] = "\\b", ["\12"] = "\\f", ["\11"] = "\\v", ["\13"] = "\\r", ["\9"] = "\\t", ["\\"] = "\\\\", ["\""] = "\\\"", ["\n"] = _86_}, {__index = _88_})
    local str0 = ("\"" .. str:gsub("[%c\\\"]", escs) .. "\"")
    if options["utf8?"] then
      return utf8_escape(str0)
    else
      return str0
    end
  end
  local function make_options(t, options)
    local defaults = {["line-length"] = 80, ["one-line?"] = false, depth = 128, ["detect-cycles?"] = true, ["empty-as-sequence?"] = false, ["metamethod?"] = true, ["prefer-colon?"] = false, ["escape-newlines?"] = false, ["utf8?"] = true, ["max-sparse-gap"] = 10}
    local overrides = {level = 0, appearances = count_table_appearances(t, {}), seen = {len = 0}}
    for k, v in pairs((options or {})) do
      defaults[k] = v
    end
    for k, v in pairs(overrides) do
      defaults[k] = v
    end
    return defaults
  end
  local function _90_(x, options, indent, colon_3f)
    local indent0 = (indent or 0)
    local options0 = (options or make_options(x))
    local x0
    if options0.preprocess then
      x0 = options0.preprocess(x, options0)
    else
      x0 = x
    end
    local tv = type(x0)
    local function _93_()
      local _92_ = getmetatable(x0)
      if (nil ~= _92_) then
        return (_92_).__fennelview
      else
        return _92_
      end
    end
    if ((tv == "table") or ((tv == "userdata") and _93_())) then
      return pp_table(x0, options0, indent0)
    elseif (tv == "number") then
      return number__3estring(x0)
    else
      local function _95_()
        if (colon_3f ~= nil) then
          return colon_3f
        elseif ("function" == type(options0["prefer-colon?"])) then
          return options0["prefer-colon?"](x0)
        else
          return options0["prefer-colon?"]
        end
      end
      if ((tv == "string") and colon_string_3f(x0) and _95_()) then
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
  pp = _90_
  local function view(x, _3foptions)
    return pp(x, make_options(x, _3foptions), 0)
  end
  return view
end
package.preload["fennel.utils"] = package.preload["fennel.utils"] or function(...)
  local view = require("fennel.view")
  local version = "1.0.0"
  local function warn(message)
    if (_G.io and _G.io.stderr) then
      return (_G.io.stderr):write(("--WARNING: %s\n"):format(tostring(message)))
    else
      return nil
    end
  end
  local function stablepairs(t)
    local keys = {}
    local used_keys = {}
    local succ = {}
    if (getmetatable(t) and getmetatable(t).keys) then
      for _, k in ipairs(getmetatable(t).keys) do
        if used_keys[k] then
          for i = #keys, 1, -1 do
            if (keys[i] == k) then
              table.remove(keys, i)
            else
            end
          end
        else
        end
        used_keys[k] = true
        table.insert(keys, k)
      end
    else
      for k in pairs(t) do
        table.insert(keys, k)
      end
      local function _100_(_241, _242)
        return (tostring(_241) < tostring(_242))
      end
      table.sort(keys, _100_)
    end
    for i, k in ipairs(keys) do
      succ[k] = keys[(i + 1)]
    end
    local function stablenext(tbl, idx)
      local key
      if (idx == nil) then
        key = keys[1]
      else
        key = succ[idx]
      end
      local value
      if (key == nil) then
        value = nil
      else
        value = tbl[key]
      end
      return key, value
    end
    return stablenext, t, nil
  end
  local function map(t, f, _3fout)
    local out = (_3fout or {})
    local f0
    if (type(f) == "function") then
      f0 = f
    else
      local function _104_(_241)
        return (_241)[f]
      end
      f0 = _104_
    end
    for _, x in ipairs(t) do
      local _106_ = f0(x)
      if (nil ~= _106_) then
        local v = _106_
        table.insert(out, v)
      else
      end
    end
    return out
  end
  local function kvmap(t, f, _3fout)
    local out = (_3fout or {})
    local f0
    if (type(f) == "function") then
      f0 = f
    else
      local function _108_(_241)
        return (_241)[f]
      end
      f0 = _108_
    end
    for k, x in stablepairs(t) do
      local _110_, _111_ = f0(k, x)
      if ((nil ~= _110_) and (nil ~= _111_)) then
        local key = _110_
        local value = _111_
        out[key] = value
      elseif (nil ~= _110_) then
        local value = _110_
        table.insert(out, value)
      else
      end
    end
    return out
  end
  local function copy(from, _3fto)
    local to = (_3fto or {})
    for k, v in pairs((from or {})) do
      to[k] = v
    end
    return to
  end
  local function member_3f(x, tbl, _3fn)
    local _113_ = tbl[(_3fn or 1)]
    if (_113_ == x) then
      return true
    elseif (_113_ == nil) then
      return nil
    elseif true then
      local _ = _113_
      return member_3f(x, tbl, ((_3fn or 1) + 1))
    else
      return nil
    end
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
        local _115_ = getmetatable(t)
        if ((_G.type(_115_) == "table") and true) then
          local __index = (_115_).__index
          if ("table" == type(__index)) then
            t = __index
            return allpairs_next(t)
          else
            return nil
          end
        else
          return nil
        end
      end
    end
    return allpairs_next
  end
  local function deref(self)
    return self[1]
  end
  local nil_sym = nil
  local function list__3estring(self, _3ftostring2)
    local safe, max = {}, 0
    for k in pairs(self) do
      if ((type(k) == "number") and (k > max)) then
        max = k
      else
      end
    end
    for i = 1, max do
      safe[i] = (((self[i] == nil) and nil_sym) or self[i])
    end
    return ("(" .. table.concat(map(safe, (_3ftostring2 or view)), " ", 1, max) .. ")")
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
  local symbol_mt = {__fennelview = deref, __tostring = deref, __eq = sym_3d, __lt = sym_3c, "SYMBOL"}
  local expr_mt
  local function _120_(x)
    return tostring(deref(x))
  end
  expr_mt = {__tostring = _120_, "EXPR"}
  local list_mt = {__fennelview = list__3estring, __tostring = list__3estring, "LIST"}
  local comment_mt = {__fennelview = comment_view, __tostring = deref, __eq = sym_3d, __lt = sym_3c, "COMMENT"}
  local sequence_marker = {"SEQUENCE"}
  local vararg = setmetatable({"..."}, {__fennelview = deref, __tostring = deref, "VARARG"})
  local getenv
  local function _121_()
    return nil
  end
  getenv = ((os and os.getenv) or _121_)
  local function debug_on_3f(flag)
    local level = (getenv("FENNEL_DEBUG") or "")
    return ((level == "all") or level:find(flag))
  end
  local function list(...)
    return setmetatable({...}, list_mt)
  end
  local function sym(str, _3fsource, _3fscope)
    local s = {["?scope"] = _3fscope, str}
    for k, v in pairs((_3fsource or {})) do
      if (type(k) == "string") then
        s[k] = v
      else
      end
    end
    return setmetatable(s, symbol_mt)
  end
  nil_sym = sym("nil")
  local function sequence(...)
    return setmetatable({...}, {sequence = sequence_marker})
  end
  local function expr(strcode, etype)
    return setmetatable({type = etype, strcode}, expr_mt)
  end
  local function comment_2a(contents, _3fsource)
    local _let_123_ = (_3fsource or {})
    local filename = _let_123_["filename"]
    local line = _let_123_["line"]
    return setmetatable({filename = filename, line = line, contents}, comment_mt)
  end
  local function varg()
    return vararg
  end
  local function expr_3f(x)
    return ((type(x) == "table") and (getmetatable(x) == expr_mt) and x)
  end
  local function varg_3f(x)
    return ((x == vararg) and x)
  end
  local function list_3f(x)
    return ((type(x) == "table") and (getmetatable(x) == list_mt) and x)
  end
  local function sym_3f(x)
    return ((type(x) == "table") and (getmetatable(x) == symbol_mt) and x)
  end
  local function sequence_3f(x)
    local mt = ((type(x) == "table") and getmetatable(x))
    return (mt and (mt.sequence == sequence_marker) and x)
  end
  local function comment_3f(x)
    return ((type(x) == "table") and (getmetatable(x) == comment_mt) and x)
  end
  local function table_3f(x)
    return ((type(x) == "table") and (x ~= vararg) and (getmetatable(x) ~= list_mt) and (getmetatable(x) ~= symbol_mt) and not comment_3f(x) and x)
  end
  local function multi_sym_3f(str)
    if sym_3f(str) then
      return multi_sym_3f(tostring(str))
    elseif (type(str) ~= "string") then
      return false
    else
      local parts = {}
      for part in str:gmatch("[^%.%:]+[%.%:]?") do
        local last_char = part:sub(( - 1))
        if (last_char == ":") then
          parts["multi-sym-method-call"] = true
        else
        end
        if ((last_char == ":") or (last_char == ".")) then
          parts[(#parts + 1)] = part:sub(1, ( - 2))
        else
          parts[(#parts + 1)] = part
        end
      end
      return ((#parts > 0) and (str:match("%.") or str:match(":")) and not str:match("%.%.") and (str:byte() ~= string.byte(".")) and (str:byte(( - 1)) ~= string.byte(".")) and parts)
    end
  end
  local function quoted_3f(symbol)
    return symbol.quoted
  end
  local function ast_source(ast)
    if table_3f(ast) then
      return (getmetatable(ast) or {})
    elseif ("table" == type(ast)) then
      return ast
    else
      return {}
    end
  end
  local function walk_tree(root, f, _3fcustom_iterator)
    local function walk(iterfn, parent, idx, node)
      if f(idx, node, parent) then
        for k, v in iterfn(node) do
          walk(iterfn, node, k, v)
        end
        return nil
      else
        return nil
      end
    end
    walk((_3fcustom_iterator or pairs), nil, nil, root)
    return root
  end
  local lua_keywords = {"and", "break", "do", "else", "elseif", "end", "false", "for", "function", "if", "in", "local", "nil", "not", "or", "repeat", "return", "then", "true", "until", "while", "goto"}
  for i, v in ipairs(lua_keywords) do
    lua_keywords[v] = i
  end
  local function valid_lua_identifier_3f(str)
    return (str:match("^[%a_][%w_]*$") and not lua_keywords[str])
  end
  local propagated_options = {"allowedGlobals", "indent", "correlate", "useMetadata", "env", "compiler-env", "compilerEnv"}
  local function propagate_options(options, subopts)
    for _, name in ipairs(propagated_options) do
      subopts[name] = options[name]
    end
    return subopts
  end
  local root
  local function _129_()
  end
  root = {chunk = nil, scope = nil, options = nil, reset = _129_}
  root["set-reset"] = function(_130_)
    local _arg_131_ = _130_
    local chunk = _arg_131_["chunk"]
    local scope = _arg_131_["scope"]
    local options = _arg_131_["options"]
    local reset = _arg_131_["reset"]
    root.reset = function()
      root.chunk, root.scope, root.options, root.reset = chunk, scope, options, reset
      return nil
    end
    return root.reset
  end
  local warned = {}
  local function check_plugin_version(_132_)
    local _arg_133_ = _132_
    local name = _arg_133_["name"]
    local versions = _arg_133_["versions"]
    local plugin = _arg_133_
    if (not member_3f(version:gsub("-dev", ""), (versions or {})) and not warned[plugin]) then
      warned[plugin] = true
      return warn(string.format("plugin %s does not support Fennel version %s", (name or "unknown"), version))
    else
      return nil
    end
  end
  local function hook(event, ...)
    local result = nil
    if (root.options and root.options.plugins) then
      for _, plugin in ipairs(root.options.plugins) do
        if result then break end
        check_plugin_version(plugin)
        local _135_ = plugin[event]
        if (nil ~= _135_) then
          local f = _135_
          result = f(...)
        else
        end
      end
    else
    end
    return result
  end
  return {warn = warn, allpairs = allpairs, stablepairs = stablepairs, copy = copy, kvmap = kvmap, map = map, ["walk-tree"] = walk_tree, ["member?"] = member_3f, list = list, sequence = sequence, sym = sym, varg = varg, expr = expr, comment = comment_2a, ["comment?"] = comment_3f, ["expr?"] = expr_3f, ["list?"] = list_3f, ["multi-sym?"] = multi_sym_3f, ["sequence?"] = sequence_3f, ["sym?"] = sym_3f, ["table?"] = table_3f, ["varg?"] = varg_3f, ["quoted?"] = quoted_3f, ["valid-lua-identifier?"] = valid_lua_identifier_3f, ["lua-keywords"] = lua_keywords, hook = hook, ["propagate-options"] = propagate_options, root = root, ["debug-on?"] = debug_on_3f, ["ast-source"] = ast_source, version = version, path = table.concat({"./?.fnl", "./?/init.fnl", getenv("FENNEL_PATH")}, ";"), ["macro-path"] = table.concat({"./?.fnl", "./?/init-macros.fnl", "./?/init.fnl", getenv("FENNEL_MACRO_PATH")}, ";")}
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
    else
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
  else
  end
  if (not opts.filename and not opts.source) then
    opts.source = str
  else
  end
  if (opts.env == "_COMPILER") then
    opts.scope = compiler["make-scope"](compiler.scopes.compiler)
  else
  end
  return opts
end
local function eval(str, options, ...)
  local opts = eval_opts(options, str)
  local env = eval_env(opts.env, opts)
  local lua_source = compiler["compile-string"](str, opts)
  local loader
  local function _616_(...)
    if opts.filename then
      return ("@" .. opts.filename)
    else
      return str
    end
  end
  loader = specials["load-code"](lua_source, env, _616_(...))
  opts.filename = nil
  return loader(...)
end
local function dofile_2a(filename, options, ...)
  local opts = utils.copy(options)
  local f = assert(io.open(filename, "rb"))
  local source = assert(f:read("*all"), ("Could not read " .. filename))
  f:close()
  opts.filename = filename
  return eval(source, opts, ...)
end
local function syntax()
  local body_3f = {"when", "with-open", "collect", "icollect", "lambda", "\206\187", "macro", "match", "accumulate"}
  local binding_3f = {"collect", "icollect", "each", "for", "let", "with-open", "accumulate"}
  local define_3f = {"fn", "lambda", "\206\187", "var", "local", "macro", "macros", "global"}
  local out = {}
  for k, v in pairs(compiler.scopes.global.specials) do
    local metadata = (compiler.metadata[v] or {})
    do end (out)[k] = {["special?"] = true, ["body-form?"] = metadata["fnl/body-form?"], ["binding-form?"] = utils["member?"](k, binding_3f), ["define?"] = utils["member?"](k, define_3f)}
  end
  for k, v in pairs(compiler.scopes.global.macros) do
    out[k] = {["macro?"] = true, ["body-form?"] = utils["member?"](k, body_3f), ["binding-form?"] = utils["member?"](k, binding_3f), ["define?"] = utils["member?"](k, define_3f)}
  end
  for k, v in pairs(_G) do
    local _617_ = type(v)
    if (_617_ == "function") then
      out[k] = {["global?"] = true, ["function?"] = true}
    elseif (_617_ == "table") then
      for k2, v2 in pairs(v) do
        if (("function" == type(v2)) and (k ~= "_G")) then
          out[(k .. "." .. k2)] = {["function?"] = true, ["global?"] = true}
        else
        end
      end
      out[k] = {["global?"] = true}
    else
    end
  end
  return out
end
local mod = {list = utils.list, ["list?"] = utils["list?"], sym = utils.sym, ["sym?"] = utils["sym?"], sequence = utils.sequence, ["sequence?"] = utils["sequence?"], comment = utils.comment, ["comment?"] = utils["comment?"], varg = utils.varg, path = utils.path, ["macro-path"] = utils["macro-path"], ["sym-char?"] = parser["sym-char?"], parser = parser.parser, granulate = parser.granulate, ["string-stream"] = parser["string-stream"], compile = compiler.compile, ["compile-string"] = compiler["compile-string"], ["compile-stream"] = compiler["compile-stream"], compile1 = compiler.compile1, traceback = compiler.traceback, mangle = compiler["global-mangling"], unmangle = compiler["global-unmangling"], metadata = compiler.metadata, scope = compiler["make-scope"], gensym = compiler.gensym, ["load-code"] = specials["load-code"], ["macro-loaded"] = specials["macro-loaded"], ["macro-searchers"] = specials["macro-searchers"], ["search-module"] = specials["search-module"], ["make-searcher"] = specials["make-searcher"], makeSearcher = specials["make-searcher"], searcher = specials["make-searcher"](), doc = specials.doc, view = view, eval = eval, dofile = dofile_2a, version = utils.version, repl = repl, syntax = syntax, loadCode = specials["load-code"], make_searcher = specials["make-searcher"], searchModule = specials["search-module"], macroLoaded = specials["macro-loaded"], compileStream = compiler["compile-stream"], compileString = compiler["compile-string"], stringStream = parser["string-stream"]}
utils["fennel-module"] = mod
do
  local builtin_macros = [===[;; This module contains all the built-in Fennel macros. Unlike all the other
  ;; modules that are loaded by the old bootstrap compiler, this runs in the
  ;; compiler scope of the version of the compiler being defined.
  
  ;; The code for these macros is somewhat idiosyncratic because it cannot use any
  ;; macros which have not yet been defined.
  
  ;; TODO: some of these macros modify their arguments; we should stop doing that,
  ;; but in a way that preserves file/line metadata.
  
  (fn ->* [val ...]
    "Thread-first macro.
  Take the first value and splice it into the second form as its first argument.
  The value of the second form is spliced into the first arg of the third, etc."
    (var x val)
    (each [_ e (ipairs [...])]
      (let [elt (if (list? e) e (list e))]
        (table.insert elt 2 x)
        (set x elt)))
    x)
  
  (fn ->>* [val ...]
    "Thread-last macro.
  Same as ->, except splices the value into the last position of each form
  rather than the first."
    (var x val)
    (each [_ e (ipairs [...])]
      (let [elt (if (list? e) e (list e))]
        (table.insert elt x)
        (set x elt)))
    x)
  
  (fn -?>* [val ...]
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
             (if (not= nil ,tmp)
                 (-?> ,el ,(unpack els))
                 ,tmp)))))
  
  (fn -?>>* [val ...]
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
             (if (not= ,tmp nil)
                 (-?>> ,el ,(unpack els))
                 ,tmp)))))
  
  (fn ?dot [tbl ...]
    "Nil-safe table look up.
  Same as . (dot), except will short-circuit with nil when it encounters
  a nil value in any of subsequent keys."
    (let [head (gensym :t)
          lookups `(do (var ,head ,tbl) ,head)]
      (each [_ k (ipairs [...])]
        ;; Kinda gnarly to reassign in place like this, but it emits the best lua.
        ;; With this impl, it emits a flat, concise, and readable set of if blocks.
        (table.insert lookups (# lookups) `(if (not= nil ,head)
                                             (set ,head (. ,head ,k)))))
      lookups))
  
  (fn doto* [val ...]
    "Evaluates val and splices it into the first argument of subsequent forms."
    (let [name (gensym)
          form `(let [,name ,val])]
      (each [_ elt (ipairs [...])]
        (let [elt (if (list? elt) elt (list elt))]
          (table.insert elt 2 name)
          (table.insert form elt)))
      (table.insert form name)
      form))
  
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
          traceback `(. (or package.loaded.fennel debug) :traceback)]
      (for [i 1 (length closable-bindings) 2]
        (assert (sym? (. closable-bindings i))
                "with-open only allows symbols in bindings")
        (table.insert closer 4 `(: ,(. closable-bindings i) :close)))
      `(let ,closable-bindings
         ,closer
         (close-handlers# (_G.xpcall ,bodyfn ,traceback)))))
  
  (fn into-val [iter-tbl]
    (var into nil)
    (for [i (length iter-tbl) 2 -1]
      (if (= :into (. iter-tbl i))
          (do (assert (not into) "expected only one :into clause")
              (set into (table.remove iter-tbl (+ i 1)))
              (table.remove iter-tbl i))))
    (assert (or (not into)
                (sym? into)
                (table? into)
                (list? into))
            "expected table, function call, or symbol in :into clause")
    (or into []))
  
  (fn collect* [iter-tbl key-expr value-expr ...]
    "Returns a table made by running an iterator and evaluating an expression that
  returns key-value pairs to be inserted sequentially into the table.  This can
  be thought of as a table comprehension. The body should provide two
  expressions (used as key and value) or nil, which causes it to be omitted from
  the resulting table.
  
  For example,
    (collect [k v (pairs {:apple \"red\" :orange \"orange\"})]
      v k)
  returns
    {:red \"apple\" :orange \"orange\"}
  
  Supports an :into clause after the iterator to put results in an existing table.
  Supports early termination with an :until clause."
    (assert (and (sequence? iter-tbl) (>= (length iter-tbl) 2))
            "expected iterator binding table")
    (assert (not= nil key-expr) "expected key and value expression")
    (assert (= nil ...)
            "expected 1 or 2 body expressions; wrap multiple expressions with do")
    (let [kv-expr (if (= nil value-expr) key-expr `(values ,key-expr ,value-expr))]
      `(let [tbl# ,(into-val iter-tbl)]
         (each ,iter-tbl
           (match ,kv-expr
             (k# v#) (tset tbl# k# v#)))
         tbl#)))
  
  (fn icollect* [iter-tbl value-expr ...]
    "Returns a sequential table made by running an iterator and evaluating an
  expression that returns values to be inserted sequentially into the table.
  This can be thought of as a \"list comprehension\". If the body returns nil
  that element is omitted from the resulting table.
  
  For example,
    (icollect [_ v (ipairs [1 2 3 4 5])] (when (not= v 3) (* v v)))
  returns
    [1 4 16 25]
  
  Supports an :into clause after the iterator to put results in an existing table.
  Supports early termination with an :until clause."
    (assert (and (sequence? iter-tbl) (>= (length iter-tbl) 2))
            "expected iterator binding table")
    (assert (not= nil value-expr) "expected table value expression")
    (assert (= nil ...)
            "expected exactly one body expression. Wrap multiple expressions with do")
    `(let [tbl# ,(into-val iter-tbl)]
       ;; believe it or not, using a var here has a pretty good performance boost:
       ;; https://p.hagelb.org/icollect-performance.html
       (var i# (length tbl#))
       (each ,iter-tbl
         (let [val# ,value-expr]
           (when (not= nil val#)
             (set i# (+ i# 1))
             (tset tbl# i# val#))))
       tbl#))
  
  (fn accumulate* [iter-tbl accum-expr ...]
    "Accumulation macro.
  It takes a binding table and an expression as its arguments.
  In the binding table, the first symbol is bound to the second value, being an
  initial accumulator variable. The rest are an iterator binding table in the
  format `each` takes.
  It runs through the iterator in each step of which the given expression is
  evaluated, and its returned value updates the accumulator variable.
  It eventually returns the final value of the accumulator variable.
  
  For example,
    (accumulate [total 0
                 _ n (pairs {:apple 2 :orange 3})]
      (+ total n))
  returns
    5"
    (assert (and (sequence? iter-tbl) (>= (length iter-tbl) 4))
            "expected initial value and iterator binding table")
    (assert (not= nil accum-expr) "expected accumulating expression")
    (assert (= nil ...)
            "expected exactly one body expression. Wrap multiple expressions with do")
    (let [accum-var (table.remove iter-tbl 1)
          accum-init (table.remove iter-tbl 1)]
      `(do (var ,accum-var ,accum-init)
           (each ,iter-tbl
             (set ,accum-var ,accum-expr))
           ,accum-var)))
  
  (fn partial* [f ...]
    "Returns a function with all arguments partially applied to f."
    (assert f "expected a function to partially apply")
    (let [bindings []
          args []]
      (each [_ arg (ipairs [...])]
        (if (or (= :number (type arg))
                (= :string (type arg))
                (= :boolean (type arg))
                (= `nil arg))
          (table.insert args arg)
          (let [name (gensym)]
            (table.insert bindings name)
            (table.insert bindings arg)
            (table.insert args name))))
      (let [body (list f (unpack args))]
        (table.insert body _VARARG)
        `(let ,bindings
           (fn [,_VARARG]
             ,body)))))
  
  (fn pick-args* [n f]
    "Creates a function of arity n that applies its arguments to f.
  
  For example,
    (pick-args 2 func)
  expands to
    (fn [_0_ _1_] (func _0_ _1_))"
    (if (and _G.io _G.io.stderr)
        (_G.io.stderr:write
         "-- WARNING: pick-args is deprecated and will be removed in the future.\n"))
    (assert (and (= (type n) :number) (= n (math.floor n)) (>= n 0))
            (.. "Expected n to be an integer literal >= 0, got " (tostring n)))
    (let [bindings []]
      (for [i 1 n]
        (tset bindings i (gensym)))
      `(fn ,bindings
         (,f ,(unpack bindings)))))
  
  (fn pick-values* [n ...]
    "Like the `values` special, but emits exactly n values.
  
  For example,
    (pick-values 2 ...)
  expands to
    (let [(_0_ _1_) ...]
      (values _0_ _1_))"
    (assert (and (= :number (type n)) (>= n 0) (= n (math.floor n)))
            (.. "Expected n to be an integer >= 0, got " (tostring n)))
    (let [let-syms (list)
          let-values (if (= 1 (select "#" ...)) ... `(values ,...))]
      (for [i 1 n]
        (table.insert let-syms (gensym)))
      (if (= n 0) `(values)
          `(let [,let-syms ,let-values]
             (values ,(unpack let-syms))))))
  
  (fn lambda* [...]
    "Function literal with nil-checked arguments.
  Like `fn`, but will throw an exception if a declared argument is passed in as
  nil, unless that argument's name begins with a question mark."
    (let [args [...]
          has-internal-name? (sym? (. args 1))
          arglist (if has-internal-name? (. args 2) (. args 1))
          docstring-position (if has-internal-name? 3 2)
          has-docstring? (and (> (length args) docstring-position)
                              (= :string (type (. args docstring-position))))
          arity-check-position (- 4 (if has-internal-name? 0 1)
                                  (if has-docstring? 0 1))
          empty-body? (< (length args) arity-check-position)]
      (fn check! [a]
        (if (table? a)
            (each [_ a (pairs a)]
              (check! a))
            (let [as (tostring a)]
              (and (not (as:match "^?")) (not= as "&") (not= as "_")
                   (not= as "...") (not= as "&as")))
            (table.insert args arity-check-position
                          `(_G.assert (not= nil ,a)
                                      ,(: "Missing argument %s on %s:%s" :format
                                          (tostring a)
                                          (or a.filename :unknown)
                                          (or a.line "?"))))))
  
      (assert (= :table (type arglist)) "expected arg list")
      (each [_ a (ipairs arglist)]
        (check! a))
      (if empty-body?
          (table.insert args (sym :nil)))
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
      `(,handle ,(view (macroexpand form _SCOPE)))))
  
  (fn import-macros* [binding1 module-name1 ...]
    "Binds a table of macros from each macro module according to a binding form.
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
            macros* (_SPECIALS.require-macros `(import-macros ,modname)
                                              scope {} binding1)]
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
  
  ;;; Pattern matching
  
  (fn match-values [vals pattern unifications match-pattern]
    (let [condition `(and)
          bindings []]
      (each [i pat (ipairs pattern)]
        (let [(subcondition subbindings) (match-pattern [(. vals i)] pat
                                                        unifications)]
          (table.insert condition subcondition)
          (each [_ b (ipairs subbindings)]
            (table.insert bindings b))))
      (values condition bindings)))
  
  (fn match-table [val pattern unifications match-pattern]
    (let [condition `(and (= (_G.type ,val) :table))
          bindings []]
      (each [k pat (pairs pattern)]
        (if (= pat `&)
            (let [rest-pat (. pattern (+ k 1))
                  rest-val `(select ,k ((or table.unpack _G.unpack) ,val))
                  subcondition (match-table `(pick-values 1 ,rest-val)
                                            rest-pat unifications match-pattern)]
              (if (not (sym? rest-pat))
                  (table.insert condition subcondition))
              (assert (= nil (. pattern (+ k 2)))
                      "expected & rest argument before last parameter")
              (table.insert bindings rest-pat)
              (table.insert bindings [rest-val]))
            (= k `&as)
            (do
              (table.insert bindings pat)
              (table.insert bindings val))
            (and (= :number (type k)) (= `&as pat))
            (do
              (assert (= nil (. pattern (+ k 2)))
                      "expected &as argument before last parameter")
              (table.insert bindings (. pattern (+ k 1)))
              (table.insert bindings val))
            ;; don't process the pattern right after &/&as; already got it
            (or (not= :number (type k)) (and (not= `&as (. pattern (- k 1)))
                                             (not= `& (. pattern (- k 1)))))
            (let [subval `(. ,val ,k)
                  (subcondition subbindings) (match-pattern [subval] pat
                                                            unifications)]
              (table.insert condition subcondition)
              (each [_ b (ipairs subbindings)]
                (table.insert bindings b)))))
      (values condition bindings)))
  
  (fn match-pattern [vals pattern unifications]
    "Takes the AST of values and a single pattern and returns a condition
  to determine if it matches as well as a list of bindings to
  introduce for the duration of the body if it does match."
    ;; we have to assume we're matching against multiple values here until we
    ;; know we're either in a multi-valued clause (in which case we know the #
    ;; of vals) or we're not, in which case we only care about the first one.
    (let [[val] vals]
      (if (or (and (sym? pattern) ; unification with outer locals (or nil)
                   (not= "_" (tostring pattern)) ; never unify _
                   (or (in-scope? pattern) (= :nil (tostring pattern))))
              (and (multi-sym? pattern) (in-scope? (. (multi-sym? pattern) 1))))
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
          ;; guard clause
          (and (list? pattern) (= (. pattern 2) `?))
          (let [(pcondition bindings) (match-pattern vals (. pattern 1)
                                                     unifications)
                condition `(and ,(unpack pattern 3))]
            (values `(and ,pcondition
                          (let ,bindings
                            ,condition)) bindings))
          ;; multi-valued patterns (represented as lists)
          (list? pattern)
          (match-values vals pattern unifications match-pattern)
          ;; table patterns
          (= (type pattern) :table)
          (match-table val pattern unifications match-pattern)
          ;; literal value
          (values `(= ,val ,pattern) []))))
  
  (fn match-condition [vals clauses]
    "Construct the actual `if` AST for the given match values and clauses."
    (if (not= 0 (% (length clauses) 2)) ; treat odd final clause as default
        (table.insert clauses (length clauses) (sym "_")))
    (let [out `(if)]
      (for [i 1 (length clauses) 2]
        (let [pattern (. clauses i)
              body (. clauses (+ i 1))
              (condition bindings) (match-pattern vals pattern {})]
          (table.insert out condition)
          (table.insert out `(let ,bindings
                               ,body))))
      out))
  
  (fn match-val-syms [clauses]
    "How many multi-valued clauses are there? return a list of that many gensyms."
    (let [syms (list (gensym))]
      (for [i 1 (length clauses) 2]
        (let [clause (if (and (list? (. clauses i)) (= `? (. clauses i 2)))
                         (. clauses i 1)
                         (. clauses i))]
          (if (list? clause)
              (each [valnum (ipairs clause)]
                (if (not (. syms valnum))
                    (tset syms valnum (gensym)))))))
      syms))
  
  (fn match* [val ...]
    ;; Old implementation of match macro, which doesn't directly support
    ;; `where' and `or'. New syntax is implemented in `match-where',
    ;; which simply generates old syntax and feeds it to `match*'.
    (let [clauses [...]
          vals (match-val-syms clauses)]
      (assert (= 0 (math.fmod (length clauses) 2))
              "expected even number of pattern/body pairs")
      ;; protect against multiple evaluation of the value, bind against as
      ;; many values as we ever match against in the clauses.
      (list `let [vals val] (match-condition vals clauses))))
  
  ;; Construction of old match syntax from new syntax
  
  (fn partition-2 [seq]
    ;; Partition `seq` by 2.
    ;; If `seq` has odd amount of elements, the last one is dropped.
    ;;
    ;; Input: [1 2 3 4 5]
    ;; Output: [[1 2] [3 4]]
    (let [firsts []
          seconds []
          res []]
      (for [i 1 (length seq) 2]
        (let [first (. seq i)
              second (. seq (+ i 1))]
          (table.insert firsts (if (not= nil first) first `nil))
          (table.insert seconds (if (not= nil second) second `nil))))
      (each [i v1 (ipairs firsts)]
        (let [v2 (. seconds i)]
          (if (not= nil v2)
              (table.insert res [v1 v2]))))
      res))
  
  (fn transform-or [[_ & pats] guards]
    ;; Transforms `(or pat pats*)` lists into match `guard` patterns.
    ;;
    ;; (or pat1 pat2), guard => [(pat1 ? guard) (pat2 ? guard)]
    (let [res []]
      (each [_ pat (ipairs pats)]
        (table.insert res (list pat `? (unpack guards))))
      res))
  
  (fn transform-cond [cond]
    ;; Transforms `where` cond into sequence of `match` guards.
    ;;
    ;; pat => [pat]
    ;; (where pat guard) => [(pat ? guard)]
    ;; (where (or pat1 pat2) guard) => [(pat1 ? guard) (pat2 ? guard)]
    (if (and (list? cond) (= (. cond 1) `where))
        (let [second (. cond 2)]
          (if (and (list? second) (= (. second 1) `or))
              (transform-or second [(unpack cond 3)])
              :else
              [(list second `? (unpack cond 3))]))
        :else
        [cond]))
  
  (fn match-where [val ...]
    "Perform pattern matching on val. See reference for details.
  
  Syntax:
  
  (match data-expression
    pattern body
    (where pattern guard guards*) body
    (where (or pattern patterns*) guard guards*) body)"
    (let [conds-bodies (partition-2 [...])
          else-branch (if (not= 0 (% (select "#" ...) 2))
                          (select (select "#" ...) ...))
          match-body []]
      (each [_ [cond body] (ipairs conds-bodies)]
        (each [_ cond (ipairs (transform-cond cond))]
          (table.insert match-body cond)
          (table.insert match-body body)))
      (if else-branch
          (table.insert match-body else-branch))
      (match* val (unpack match-body))))
  
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
   :accumulate accumulate*
   :partial partial*
   :lambda lambda*
   :pick-args pick-args*
   :pick-values pick-values*
   :macro macro*
   :macrodebug macrodebug*
   :import-macros import-macros*
   :match match-where}
  ]===]
  local module_name = "fennel.macros"
  local _
  local function _620_()
    return mod
  end
  package.preload[module_name] = _620_
  _ = nil
  local env
  do
    local _621_ = specials["make-compiler-env"](nil, compiler.scopes.compiler, {})
    do end (_621_)["utils"] = utils
    _621_["fennel"] = mod
    env = _621_
  end
  local built_ins = eval(builtin_macros, {env = env, scope = compiler.scopes.compiler, allowedGlobals = false, useMetadata = true, filename = "src/fennel/macros.fnl", moduleName = module_name})
  for k, v in pairs(built_ins) do
    compiler.scopes.global.macros[k] = v
  end
  compiler.scopes.global.macros["\206\187"] = compiler.scopes.global.macros.lambda
  package.preload[module_name] = nil
end
return mod
