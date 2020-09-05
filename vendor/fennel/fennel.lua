package.preload["fennel.repl"] = package.preload["fennel.repl"] or function(...)
  local utils = require("fennel.utils")
  local parser = require("fennel.parser")
  local compiler = require("fennel.compiler")
  local specials = require("fennel.specials")
  local function default_read_chunk(parser_state)
    local function _0_()
      if (0 < parser_state["stack-size"]) then
        return ".."
      else
        return ">> "
      end
    end
    io.write(_0_())
    io.flush()
    local input = io.read()
    return (input and (input .. "\n"))
  end
  local function default_on_values(xs)
    io.write(table.concat(xs, "\9"))
    return io.write("\n")
  end
  local function default_on_error(errtype, err, lua_source)
    local function _1_()
      local _0_0 = errtype
      if (_0_0 == "Lua Compile") then
        return ("Bad code generated - likely a bug with the compiler:\n" .. "--- Generated Lua Start ---\n" .. lua_source .. "--- Generated Lua End ---\n")
      elseif (_0_0 == "Runtime") then
        return (compiler.traceback(err, 4) .. "\n")
      else
        local _ = _0_0
        return ("%s error: %s\n"):format(errtype, tostring(err))
      end
    end
    return io.write(_1_())
  end
  local save_source = table.concat({"local ___i___ = 1", "while true do", " local name, value = debug.getlocal(1, ___i___)", " if(name and name ~= \"___i___\") then", " ___replLocals___[name] = value", " ___i___ = ___i___ + 1", " else break end end"}, "\n")
  local function splice_save_locals(env, lua_source)
    env.___replLocals___ = (env.___replLocals___ or {})
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
    end
    return table.concat(spliced_source, "\n")
  end
  local function completer(env, scope, text)
    local matches = {}
    local input_fragment = text:gsub(".*[%s)(]+", "")
    local function add_partials(input, tbl, prefix)
      for k in utils.allpairs(tbl) do
        local k0 = nil
        if ((tbl == env) or (tbl == env.___replLocals___)) then
          k0 = scope.unmanglings[k]
        else
          k0 = k
        end
        if ((#matches < 2000) and (type(k0) == "string") and (input == k0:sub(0, #input))) then
          table.insert(matches, (prefix .. k0))
        end
      end
      return nil
    end
    local function add_matches(input, tbl, prefix)
      local prefix0 = nil
      if prefix then
        prefix0 = (prefix .. ".")
      else
        prefix0 = ""
      end
      if not input:find("%.") then
        return add_partials(input, tbl, prefix0)
      else
        local head, tail = input:match("^([^.]+)%.(.*)")
        local raw_head = nil
        if ((tbl == env) or (tbl == env.___replLocals___)) then
          raw_head = scope.manglings[head]
        else
          raw_head = head
        end
        if (type(tbl[raw_head]) == "table") then
          return add_matches(tail, tbl[raw_head], (prefix0 .. head))
        end
      end
    end
    add_matches(input_fragment, (scope.specials or {}))
    add_matches(input_fragment, (scope.macros or {}))
    add_matches(input_fragment, (env.___replLocals___ or {}))
    add_matches(input_fragment, env)
    add_matches(input_fragment, (env._ENV or env._G or {}))
    return matches
  end
  local function repl(options)
    local old_root_options = utils.root.options
    local env = nil
    if options.env then
      env = specials["wrap-env"](options.env)
    else
      env = setmetatable({}, {__index = (_G._ENV or _G)})
    end
    local save_locals_3f = ((options.saveLocals ~= false) and env.debug and env.debug.getlocal)
    local opts = {}
    local _ = nil
    for k, v in pairs(options) do
      opts[k] = v
    end
    _ = nil
    local read_chunk = (opts.readChunk or default_read_chunk)
    local on_values = (opts.onValues or default_on_values)
    local on_error = (opts.onError or default_on_error)
    local pp = (opts.pp or tostring)
    local byte_stream, clear_stream = parser.granulate(read_chunk)
    local chars = {}
    local read, reset = nil, nil
    local function _1_(parser_state)
      local c = byte_stream(parser_state)
      chars[(#chars + 1)] = c
      return c
    end
    read, reset = parser.parser(_1_)
    local scope = compiler["make-scope"]()
    opts.useMetadata = (options.useMetadata ~= false)
    if (opts.allowedGlobals == nil) then
      opts.allowedGlobals = specials["current-global-names"](opts.env)
    end
    if opts.registerCompleter then
      local function _3_(...)
        return completer(env, scope, ...)
      end
      opts.registerCompleter(_3_)
    end
    local function loop()
      for k in pairs(chars) do
        chars[k] = nil
      end
      local ok, parse_ok_3f, x = pcall(read)
      local src_string = string.char((_G.unpack or table.unpack)(chars))
      utils.root.options = opts
      if not ok then
        on_error("Parse", parse_ok_3f)
        clear_stream()
        reset()
        return loop()
      else
        if parse_ok_3f then
          do
            local _4_0, _5_0 = pcall(compiler.compile, x, {["assert-compile"] = opts["assert-compile"], ["parse-error"] = opts["parse-error"], correlate = opts.correlate, moduleName = opts.moduleName, scope = scope, source = src_string, useMetadata = opts.useMetadata})
            if ((_4_0 == false) and (nil ~= _5_0)) then
              local msg = _5_0
              clear_stream()
              on_error("Compile", msg)
            elseif ((_4_0 == true) and (nil ~= _5_0)) then
              local source = _5_0
              local source0 = nil
              if save_locals_3f then
                source0 = splice_save_locals(env, source)
              else
                source0 = source
              end
              local lua_ok_3f, loader = pcall(specials["load-code"], source0, env)
              if not lua_ok_3f then
                clear_stream()
                on_error("Lua Compile", loader, source0)
              else
                local _7_0, _8_0 = nil, nil
                local function _9_()
                  return {loader()}
                end
                local function _10_(...)
                  return on_error("Runtime", ...)
                end
                _7_0, _8_0 = xpcall(_9_, _10_)
                if ((_7_0 == true) and (nil ~= _8_0)) then
                  local ret = _8_0
                  env._ = ret[1]
                  env.__ = ret
                  on_values(utils.map(ret, pp))
                end
              end
            end
          end
          utils.root.options = old_root_options
          return loop()
        end
      end
    end
    return loop()
  end
  return repl
end
package.preload["fennel.specials"] = package.preload["fennel.specials"] or function(...)
  local utils = require("fennel.utils")
  local parser = require("fennel.parser")
  local compiler = require("fennel.compiler")
  local unpack = (_G.unpack or table.unpack)
  local SPECIALS = compiler.scopes.global.specials
  local function wrap_env(env)
    local function _0_(_, key)
      if (type(key) == "string") then
        return env[compiler["global-unmangling"](key)]
      else
        return env[key]
      end
    end
    local function _1_(_, key, value)
      if (type(key) == "string") then
        env[compiler["global-unmangling"](key)] = value
        return nil
      else
        env[key] = value
        return nil
      end
    end
    local function _2_()
      local function putenv(k, v)
        local _3_
        if (type(k) == "string") then
          _3_ = compiler["global-unmangling"](k)
        else
          _3_ = k
        end
        return _3_, v
      end
      return next, utils.kvmap(env, putenv), nil
    end
    return setmetatable({}, {__index = _0_, __newindex = _1_, __pairs = _2_})
  end
  local function current_global_names(env)
    return utils.kvmap((env or _G), compiler["global-unmangling"])
  end
  local function load_code(code, environment, filename)
    local environment0 = ((environment or _ENV) or _G)
    if (_G.setfenv and _G.loadstring) then
      local f = assert(_G.loadstring(code, filename))
      _G.setfenv(f, environment0)
      return f
    else
      return assert(load(code, filename, "t", environment0))
    end
  end
  local function doc_2a(tgt, name)
    if not tgt then
      return (name .. " not found")
    else
      local docstring = (((compiler.metadata):get(tgt, "fnl/docstring") or "#<undocumented>")):gsub("\n$", ""):gsub("\n", "\n  ")
      if (type(tgt) == "function") then
        local arglist = table.concat(((compiler.metadata):get(tgt, "fnl/arglist") or {"#<unknown-arguments>"}), " ")
        local _0_
        if (#arglist > 0) then
          _0_ = " "
        else
          _0_ = ""
        end
        return string.format("(%s%s%s)\n  %s", name, _0_, arglist, docstring)
      else
        return string.format("%s\n  %s", name, docstring)
      end
    end
  end
  local function doc_special(name, arglist, docstring)
    compiler.metadata[SPECIALS[name]] = {["fnl/arglist"] = arglist, ["fnl/docstring"] = docstring}
    return nil
  end
  local function compile_do(ast, scope, parent, start)
    local start0 = (start or 2)
    local len = #ast
    local sub_scope = compiler["make-scope"](scope)
    for i = start0, len do
      compiler.compile1(ast[i], sub_scope, parent, {nval = 0})
    end
    return nil
  end
  SPECIALS["do"] = function(ast, scope, parent, opts, start, chunk, sub_scope, pre_syms)
    local start0 = (start or 2)
    local sub_scope0 = (sub_scope or compiler["make-scope"](scope))
    local chunk0 = (chunk or {})
    local len = #ast
    local retexprs = {returned = true}
    local function compile_body(outer_target, outer_tail, outer_retexprs)
      if (len < start0) then
        compiler.compile1(nil, sub_scope0, chunk0, {tail = outer_tail, target = outer_target})
      else
        for i = start0, len do
          local subopts = {nval = (((i ~= len) and 0) or opts.nval), tail = (((i == len) and outer_tail) or nil), target = (((i == len) and outer_target) or nil)}
          local _ = utils["propagate-options"](opts, subopts)
          local subexprs = compiler.compile1(ast[i], sub_scope0, chunk0, subopts)
          if (i ~= len) then
            compiler["keep-side-effects"](subexprs, parent, nil, ast[i])
          end
        end
      end
      compiler.emit(parent, chunk0, ast)
      compiler.emit(parent, "end", ast)
      return (outer_retexprs or retexprs)
    end
    if (opts.target or (opts.nval == 0) or opts.tail) then
      compiler.emit(parent, "do", ast)
      return compile_body(opts.target, opts.tail)
    elseif opts.nval then
      local syms = {}
      for i = 1, opts.nval, 1 do
        local s = ((pre_syms and pre_syms[i]) or compiler.gensym(scope))
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
      utils.hook("do", ast, sub_scope0)
      return compile_body(nil, true, utils.expr((fname .. "(" .. fargs .. ")"), "statement"))
    end
  end
  doc_special("do", {"..."}, "Evaluate multiple forms; return last value.")
  SPECIALS.values = function(ast, scope, parent)
    local len = #ast
    local exprs = {}
    for i = 2, len do
      local subexprs = compiler.compile1(ast[i], scope, parent, {nval = ((i ~= len) and 1)})
      exprs[(#exprs + 1)] = subexprs[1]
      if (i == len) then
        for j = 2, #subexprs, 1 do
          exprs[(#exprs + 1)] = subexprs[j]
        end
      end
    end
    return exprs
  end
  doc_special("values", {"..."}, "Return multiple values from a function. Must be in tail position.")
  local function set_fn_metadata(arg_list, docstring, parent, fn_name)
    if utils.root.options.useMetadata then
      local args = nil
      local function _0_(v)
        if utils["table?"](v) then
          return "\"#<table>\""
        else
          return ("\"%s\""):format(tostring(v))
        end
      end
      args = utils.map(arg_list, _0_)
      local meta_fields = {"\"fnl/arglist\"", ("{" .. table.concat(args, ", ") .. "}")}
      if docstring then
        table.insert(meta_fields, "\"fnl/docstring\"")
        table.insert(meta_fields, ("\"" .. docstring:gsub("%s+$", ""):gsub("\\", "\\\\"):gsub("\n", "\\n"):gsub("\"", "\\\"") .. "\""))
      end
      local meta_str = ("require(\"%s\").metadata"):format((utils.root.options.moduleName or "fennel"))
      return compiler.emit(parent, ("pcall(function() %s:setall(%s, %s) end)"):format(meta_str, fn_name, table.concat(meta_fields, ", ")))
    end
  end
  local function get_fn_name(ast, scope, fn_name, multi)
    if (fn_name and (fn_name[1] ~= "nil")) then
      local _0_
      if not multi then
        _0_ = compiler["declare-local"](fn_name, {}, scope, ast)
      else
        _0_ = compiler["symbol-to-expression"](fn_name, scope)[1]
      end
      return _0_, not multi, 3
    else
      return compiler.gensym(scope), true, 2
    end
  end
  SPECIALS.fn = function(ast, scope, parent)
    local f_scope = nil
    do
      local _0_0 = compiler["make-scope"](scope)
      _0_0["vararg"] = false
      f_scope = _0_0
    end
    local f_chunk = {}
    local fn_sym = utils["sym?"](ast[2])
    local multi = (fn_sym and utils["multi-sym?"](fn_sym[1]))
    local fn_name, local_fn_3f, index = get_fn_name(ast, scope, fn_sym, multi)
    local arg_list = compiler.assert(utils["table?"](ast[index]), "expected parameters table", ast)
    compiler.assert((not multi or not multi["multi-sym-method-call"]), ("unexpected multi symbol " .. tostring(fn_name)), fn_sym)
    local function get_arg_name(arg)
      if utils["varg?"](arg) then
        compiler.assert((arg == arg_list[#arg_list]), "expected vararg as last parameter", ast)
        f_scope.vararg = true
        return "..."
      elseif (utils["sym?"](arg) and (utils.deref(arg) ~= "nil") and not utils["multi-sym?"](utils.deref(arg))) then
        return compiler["declare-local"](arg, {}, f_scope, ast)
      elseif utils["table?"](arg) then
        local raw = utils.sym(compiler.gensym(scope))
        local declared = compiler["declare-local"](raw, {}, f_scope, ast)
        compiler.destructure(arg, raw, ast, f_scope, f_chunk, {declaration = true, nomulti = true})
        return declared
      else
        return compiler.assert(false, ("expected symbol for function parameter: %s"):format(tostring(arg)), ast[2])
      end
    end
    do
      local arg_name_list = utils.map(arg_list, get_arg_name)
      local index0, docstring = nil, nil
      if ((type(ast[(index + 1)]) == "string") and ((index + 1) < #ast)) then
        index0, docstring = (index + 1), ast[(index + 1)]
      else
        index0, docstring = index, nil
      end
      for i = (index0 + 1), #ast, 1 do
        compiler.compile1(ast[i], f_scope, f_chunk, {nval = (((i ~= #ast) and 0) or nil), tail = (i == #ast)})
      end
      if local_fn_3f then
        compiler.emit(parent, ("local function %s(%s)"):format(fn_name, table.concat(arg_name_list, ", ")), ast)
      else
        compiler.emit(parent, ("%s = function(%s)"):format(fn_name, table.concat(arg_name_list, ", ")), ast)
      end
      compiler.emit(parent, f_chunk, ast)
      compiler.emit(parent, "end", ast)
      set_fn_metadata(arg_list, docstring, parent, fn_name)
    end
    utils.hook("fn", ast, f_scope)
    return utils.expr(fn_name, "sym")
  end
  doc_special("fn", {"name?", "args", "docstring?", "..."}, "Function syntax. May optionally include a name and docstring.\nIf a name is provided, the function will be bound in the current scope.\nWhen called with the wrong number of args, excess args will be discarded\nand lacking args will be nil, use lambda for arity-checked functions.")
  SPECIALS.lua = function(ast, _, parent)
    compiler.assert(((#ast == 2) or (#ast == 3)), "expected 1 or 2 arguments", ast)
    if (ast[2] ~= nil) then
      table.insert(parent, {ast = ast, leaf = tostring(ast[2])})
    end
    if (#ast == 3) then
      return tostring(ast[3])
    end
  end
  SPECIALS.doc = function(ast, scope, parent)
    assert(utils.root.options.useMetadata, "can't look up doc with metadata disabled.")
    compiler.assert((#ast == 2), "expected one argument", ast)
    local target = utils.deref(ast[2])
    local special_or_macro = (scope.specials[target] or scope.macros[target])
    if special_or_macro then
      return ("print([[%s]])"):format(doc_2a(special_or_macro, target))
    else
      local value = tostring(compiler.compile1(ast[2], scope, parent, {nval = 1})[1])
      return ("print(require('%s').doc(%s, '%s'))"):format((utils.root.options.moduleName or "fennel"), value, tostring(ast[2]))
    end
  end
  doc_special("doc", {"x"}, "Print the docstring and arglist for a function, macro, or special form.")
  local function dot(ast, scope, parent)
    compiler.assert((1 < #ast), "expected table argument", ast)
    local len = #ast
    local lhs = compiler.compile1(ast[2], scope, parent, {nval = 1})
    if (len == 2) then
      return tostring(lhs[1])
    else
      local indices = {}
      for i = 3, len, 1 do
        local index = ast[i]
        if ((type(index) == "string") and utils["valid-lua-identifier?"](index)) then
          table.insert(indices, ("." .. index))
        else
          local _0_ = compiler.compile1(index, scope, parent, {nval = 1})
          local index0 = _0_[1]
          table.insert(indices, ("[" .. tostring(index0) .. "]"))
        end
      end
      if utils["table?"](ast[2]) then
        return ("(" .. tostring(lhs[1]) .. ")" .. table.concat(indices))
      else
        return (tostring(lhs[1]) .. table.concat(indices))
      end
    end
  end
  SPECIALS["."] = dot
  doc_special(".", {"tbl", "key1", "..."}, "Look up key1 in tbl table. If more args are provided, do a nested lookup.")
  SPECIALS.global = function(ast, scope, parent)
    compiler.assert((#ast == 3), "expected name and value", ast)
    compiler.destructure(ast[2], ast[3], ast, scope, parent, {forceglobal = true, nomulti = true})
    return nil
  end
  doc_special("global", {"name", "val"}, "Set name as a global with val.")
  SPECIALS.set = function(ast, scope, parent)
    compiler.assert((#ast == 3), "expected name and value", ast)
    compiler.destructure(ast[2], ast[3], ast, scope, parent, {noundef = true})
    return nil
  end
  doc_special("set", {"name", "val"}, "Set a local variable to a new value. Only works on locals using var.")
  local function set_forcibly_21_2a(ast, scope, parent)
    compiler.assert((#ast == 3), "expected name and value", ast)
    compiler.destructure(ast[2], ast[3], ast, scope, parent, {forceset = true})
    return nil
  end
  SPECIALS["set-forcibly!"] = set_forcibly_21_2a
  local function local_2a(ast, scope, parent)
    compiler.assert((#ast == 3), "expected name and value", ast)
    compiler.destructure(ast[2], ast[3], ast, scope, parent, {declaration = true, nomulti = true})
    return nil
  end
  SPECIALS["local"] = local_2a
  doc_special("local", {"name", "val"}, "Introduce new top-level immutable local.")
  SPECIALS.var = function(ast, scope, parent)
    compiler.assert((#ast == 3), "expected name and value", ast)
    compiler.destructure(ast[2], ast[3], ast, scope, parent, {declaration = true, isvar = true, nomulti = true})
    return nil
  end
  doc_special("var", {"name", "val"}, "Introduce new mutable local.")
  SPECIALS.let = function(ast, scope, parent, opts)
    local bindings = ast[2]
    local pre_syms = {}
    compiler.assert((utils["list?"](bindings) or utils["table?"](bindings)), "expected binding table", ast)
    compiler.assert(((#bindings % 2) == 0), "expected even number of name/value bindings", ast[2])
    compiler.assert((#ast >= 3), "expected body expression", ast[1])
    for _ = 1, (opts.nval or 0), 1 do
      table.insert(pre_syms, compiler.gensym(scope))
    end
    local sub_scope = compiler["make-scope"](scope)
    local sub_chunk = {}
    for i = 1, #bindings, 2 do
      compiler.destructure(bindings[i], bindings[(i + 1)], ast, sub_scope, sub_chunk, {declaration = true, nomulti = true})
    end
    return SPECIALS["do"](ast, scope, parent, opts, 3, sub_chunk, sub_scope, pre_syms)
  end
  doc_special("let", {"[name1 val1 ... nameN valN]", "..."}, "Introduces a new scope in which a given set of local bindings are used.")
  SPECIALS.tset = function(ast, scope, parent)
    compiler.assert((#ast > 3), "expected table, key, and value arguments", ast)
    local root = compiler.compile1(ast[2], scope, parent, {nval = 1})[1]
    local keys = {}
    for i = 3, (#ast - 1), 1 do
      local key = compiler.compile1(ast[i], scope, parent, {nval = 1})[1]
      keys[(#keys + 1)] = tostring(key)
    end
    local value = compiler.compile1(ast[#ast], scope, parent, {nval = 1})[1]
    local rootstr = tostring(root)
    local fmtstr = nil
    if rootstr:match("^{") then
      fmtstr = "do end (%s)[%s] = %s"
    else
      fmtstr = "%s[%s] = %s"
    end
    return compiler.emit(parent, fmtstr:format(tostring(root), table.concat(keys, "]["), tostring(value)), ast)
  end
  doc_special("tset", {"tbl", "key1", "...", "keyN", "val"}, "Set the value of a table field. Can take additional keys to set\nnested values, but all parents must contain an existing table.")
  local function calculate_target(scope, opts)
    if not (opts.tail or opts.target or opts.nval) then
      return "iife", true, nil
    elseif (opts.nval and (opts.nval ~= 0) and not opts.target) then
      local accum = {}
      local target_exprs = {}
      for i = 1, opts.nval, 1 do
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
    local has_else_3f = ((#ast > 3) and ((#ast % 2) == 0))
    local else_branch = (has_else_3f and compile_body(#ast))
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
      local cond_line = nil
      if ((cond == "true") and branch.nested and (i == #branches)) then
        cond_line = "else"
      else
        cond_line = fstr:format(cond)
      end
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
        if has_else_3f then
          compiler.emit(last_buffer, "else", ast)
          compiler.emit(last_buffer, else_branch.chunk, ast)
        elseif (inner_target and (cond_line ~= "else")) then
          compiler.emit(last_buffer, "else", ast)
          compiler.emit(last_buffer, ("%s = nil"):format(inner_target), ast)
        end
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
      for i = 1, #buffer, 1 do
        compiler.emit(parent, buffer[i], ast)
      end
      return {returned = true}
    else
      compiler.emit(parent, ("local %s"):format(inner_target), ast)
      for i = 1, #buffer, 1 do
        compiler.emit(parent, buffer[i], ast)
      end
      return target_exprs
    end
  end
  SPECIALS["if"] = if_2a
  doc_special("if", {"cond1", "body1", "...", "condN", "bodyN"}, "Conditional form.\nTakes any number of condition/body pairs and evaluates the first body where\nthe condition evaluates to truthy. Similar to cond in other lisps.")
  SPECIALS.each = function(ast, scope, parent)
    compiler.assert((#ast >= 3), "expected body expression", ast[1])
    local binding = compiler.assert(utils["table?"](ast[2]), "expected binding table", ast)
    local iter = table.remove(binding, #binding)
    local destructures = {}
    local new_manglings = {}
    local sub_scope = compiler["make-scope"](scope)
    local function destructure_binding(v)
      if utils["sym?"](v) then
        return compiler["declare-local"](v, {}, sub_scope, ast, new_manglings)
      else
        local raw = utils.sym(compiler.gensym(sub_scope))
        destructures[raw] = v
        return compiler["declare-local"](raw, {}, sub_scope, ast)
      end
    end
    local bind_vars = utils.map(binding, destructure_binding)
    local vals = compiler.compile1(iter, sub_scope, parent)
    local val_names = utils.map(vals, tostring)
    local chunk = {}
    compiler.emit(parent, ("for %s in %s do"):format(table.concat(bind_vars, ", "), table.concat(val_names, ", ")), ast)
    for raw, args in utils.stablepairs(destructures) do
      compiler.destructure(args, raw, ast, sub_scope, chunk, {declaration = true, nomulti = true})
    end
    compiler["apply-manglings"](sub_scope, new_manglings, ast)
    compile_do(ast, sub_scope, chunk, 3)
    compiler.emit(parent, chunk, ast)
    return compiler.emit(parent, "end", ast)
  end
  doc_special("each", {"[key value (iterator)]", "..."}, "Runs the body once for each set of values provided by the given iterator.\nMost commonly used with ipairs for sequential tables or pairs for  undefined\norder, but can be used with any iterator.")
  local function while_2a(ast, scope, parent)
    local len1 = #parent
    local condition = compiler.compile1(ast[2], scope, parent, {nval = 1})[1]
    local len2 = #parent
    local sub_chunk = {}
    if (len1 ~= len2) then
      for i = (len1 + 1), len2, 1 do
        sub_chunk[(#sub_chunk + 1)] = parent[i]
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
  doc_special("while", {"condition", "..."}, "The classic while loop. Evaluates body until a condition is non-truthy.")
  local function for_2a(ast, scope, parent)
    local ranges = compiler.assert(utils["table?"](ast[2]), "expected binding table", ast)
    local binding_sym = table.remove(ast[2], 1)
    local sub_scope = compiler["make-scope"](scope)
    local range_args = {}
    local chunk = {}
    compiler.assert(utils["sym?"](binding_sym), ("unable to bind %s %s"):format(type(binding_sym), tostring(binding_sym)), ast[2])
    compiler.assert((#ast >= 3), "expected body expression", ast[1])
    for i = 1, math.min(#ranges, 3), 1 do
      range_args[i] = tostring(compiler.compile1(ranges[i], sub_scope, parent, {nval = 1})[1])
    end
    compiler.emit(parent, ("for %s = %s do"):format(compiler["declare-local"](binding_sym, {}, sub_scope, ast), table.concat(range_args, ", ")), ast)
    compile_do(ast, sub_scope, chunk, 3)
    compiler.emit(parent, chunk, ast)
    return compiler.emit(parent, "end", ast)
  end
  SPECIALS["for"] = for_2a
  doc_special("for", {"[index start stop step?]", "..."}, "Numeric loop construct.\nEvaluates body once for each value between start and stop (inclusive).")
  local function native_method_call(ast, scope, parent, target, args)
    local _0_ = ast
    local _ = _0_[1]
    local _0 = _0_[2]
    local method_string = _0_[3]
    local call_string = nil
    if ((target.type == "literal") or (target.type == "expression")) then
      call_string = "(%s):%s(%s)"
    else
      call_string = "%s:%s(%s)"
    end
    return utils.expr(string.format(call_string, tostring(target), method_string, table.concat(args, ", ")), "statement")
  end
  local function nonnative_method_call(ast, scope, parent, target, args)
    local method_string = tostring(compiler.compile1(ast[3], scope, parent, {nval = 1})[1])
    table.insert(args, tostring(target))
    return utils.expr(string.format("%s[%s](%s)", tostring(target), method_string, tostring(target), table.concat(args, ", ")), "statement")
  end
  local function double_eval_protected_method_call(ast, scope, parent, target, args)
    local method_string = tostring(compiler.compile1(ast[3], scope, parent, {nval = 1})[1])
    local call = "(function(tgt, m, ...) return tgt[m](tgt, ...) end)(%s, %s)"
    table.insert(args, 1, method_string)
    return utils.expr(string.format(call, tostring(target), table.concat(args, ", ")), "statement")
  end
  local function method_call(ast, scope, parent)
    compiler.assert((2 < #ast), "expected at least 2 arguments", ast)
    local _0_ = compiler.compile1(ast[2], scope, parent, {nval = 1})
    local target = _0_[1]
    local args = {}
    for i = 4, #ast do
      local subexprs = nil
      local _1_
      if (i ~= #ast) then
        _1_ = 1
      else
      _1_ = nil
      end
      subexprs = compiler.compile1(ast[i], scope, parent, {nval = _1_})
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
    for i = 2, #ast, 1 do
      els[(#els + 1)] = tostring(ast[i]):gsub("\n", " ")
    end
    return compiler.emit(parent, ("-- " .. table.concat(els, " ")), ast)
  end
  doc_special("comment", {"..."}, "Comment which will be emitted in Lua output.")
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
      local _0_0 = compiler["make-scope"](scope)
      _0_0["vararg"] = false
      _0_0["hashfn"] = true
      f_scope = _0_0
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
      if (utils["sym?"](node) and (utils.deref(node) == "$...")) then
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
    end
    local arg_str = nil
    if f_scope.vararg then
      arg_str = utils.deref(utils.varg())
    else
      arg_str = table.concat(args, ", ", 1, max_used)
    end
    compiler.emit(parent, string.format("local function %s(%s)", name, arg_str), ast)
    compiler.emit(parent, f_chunk, ast)
    compiler.emit(parent, "end", ast)
    return utils.expr(name, "sym")
  end
  doc_special("hashfn", {"..."}, "Function literal shorthand; args are either $... OR $1, $2, etc.")
  local function define_arithmetic_special(name, zero_arity, unary_prefix, lua_name)
    do
      local padded_op = (" " .. (lua_name or name) .. " ")
      local function _0_(ast, scope, parent)
        local len = #ast
        if (len == 1) then
          compiler.assert((zero_arity ~= nil), "Expected more than 0 arguments", ast)
          return utils.expr(zero_arity, "literal")
        else
          local operands = {}
          for i = 2, len, 1 do
            local subexprs = nil
            local _1_
            if (i == 1) then
              _1_ = 1
            else
            _1_ = nil
            end
            subexprs = compiler.compile1(ast[i], scope, parent, {nval = _1_})
            utils.map(subexprs, tostring, operands)
          end
          if (#operands == 1) then
            if unary_prefix then
              return ("(" .. unary_prefix .. padded_op .. operands[1] .. ")")
            else
              return operands[1]
            end
          else
            return ("(" .. table.concat(operands, padded_op) .. ")")
          end
        end
      end
      SPECIALS[name] = _0_
    end
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
  define_arithmetic_special("lshift", nil, "1", "<<")
  define_arithmetic_special("rshift", nil, "1", ">>")
  define_arithmetic_special("band", "0", "0", "&")
  define_arithmetic_special("bor", "0", "0", "|")
  define_arithmetic_special("bxor", "0", "0", "~")
  doc_special("lshift", {"x", "n"}, "Bitwise logical left shift of x by n bits; only works in Lua 5.3+.")
  doc_special("rshift", {"x", "n"}, "Bitwise logical right shift of x by n bits; only works in Lua 5.3+.")
  doc_special("band", {"x1", "x2"}, "Bitwise AND of arguments; only works in Lua 5.3+.")
  doc_special("bor", {"x1", "x2"}, "Bitwise OR of arguments; only works in Lua 5.3+.")
  doc_special("bxor", {"x1", "x2"}, "Bitwise XOR of arguments; only works in Lua 5.3+.")
  define_arithmetic_special("or", "false")
  define_arithmetic_special("and", "true")
  doc_special("and", {"a", "b", "..."}, "Boolean operator; works the same as Lua but accepts more arguments.")
  doc_special("or", {"a", "b", "..."}, "Boolean operator; works the same as Lua but accepts more arguments.")
  doc_special("..", {"a", "b", "..."}, "String concatenation operator; works the same as Lua but accepts more arguments.")
  local function native_comparator(op, _0_0, scope, parent)
    local _1_ = _0_0
    local _ = _1_[1]
    local lhs_ast = _1_[2]
    local rhs_ast = _1_[3]
    local _2_ = compiler.compile1(lhs_ast, scope, parent, {nval = 1})
    local lhs = _2_[1]
    local _3_ = compiler.compile1(rhs_ast, scope, parent, {nval = 1})
    local rhs = _3_[1]
    return string.format("(%s %s %s)", tostring(lhs), op, tostring(rhs))
  end
  local function double_eval_protected_comparator(op, chain_op, ast, scope, parent)
    local arglist = {}
    local comparisons = {}
    local vals = {}
    local chain = string.format(" %s ", (chain_op or "and"))
    for i = 2, #ast do
      table.insert(arglist, tostring(compiler.gensym(scope)))
      table.insert(vals, tostring(compiler.compile1(ast[i], scope, parent, {nval = 1})[1]))
    end
    for i = 1, (#arglist - 1) do
      table.insert(comparisons, string.format("(%s %s %s)", arglist[i], op, arglist[(i + 1)]))
    end
    return string.format("(function(%s) return %s end)(%s)", table.concat(arglist, ","), table.concat(comparisons, chain), table.concat(vals, ","))
  end
  local function define_comparator_special(name, lua_op, chain_op)
    do
      local op = (lua_op or name)
      local function opfn(ast, scope, parent)
        compiler.assert((2 < #ast), "expected at least two arguments", ast)
        if (3 == #ast) then
          return native_comparator(op, ast, scope, parent)
        else
          return double_eval_protected_comparator(op, chain_op, ast, scope, parent)
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
  SPECIALS["~="] = SPECIALS["not="]
  local function define_unary_special(op, realop)
    local function opfn(ast, scope, parent)
      compiler.assert((#ast == 2), "expected one argument", ast)
      local tail = compiler.compile1(ast[2], scope, parent, {nval = 1})
      return ((realop or op) .. tostring(tail[1]))
    end
    SPECIALS[op] = opfn
    return nil
  end
  define_unary_special("not", "not ")
  doc_special("not", {"x"}, "Logical operator; works the same as Lua.")
  define_unary_special("bnot", "~")
  doc_special("bnot", {"x"}, "Bitwise negation; only works in Lua 5.3+.")
  define_unary_special("length", "#")
  doc_special("length", {"x"}, "Returns the length of a table or string.")
  SPECIALS["#"] = SPECIALS.length
  SPECIALS.quote = function(ast, scope, parent)
    compiler.assert((#ast == 2), "expected one argument")
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
  local already_warned_3f = {}
  local compile_env_warning = ("WARNING: Attempting to %s %s in compile" .. " scope.\nIn future versions of Fennel this will not" .. " be allowed without the\n--no-compiler-sandbox flag" .. " or passing :compiler-env _G in options.\n")
  local function compiler_env_warn(env, key)
    local v = _G[key]
    if (v and io and io.stderr and not already_warned_3f[key]) then
      already_warned_3f[key] = true
      do end (io.stderr):write(compile_env_warning:format("use global", key))
    end
    return v
  end
  local safe_compiler_env = setmetatable({assert = assert, bit = _G.bit, error = error, getmetatable = getmetatable, ipairs = ipairs, math = math, next = next, pairs = pairs, pcall = pcall, print = print, select = select, setmetatable = setmetatable, string = string, table = table, tonumber = tonumber, tostring = tostring, type = type, xpcall = xpcall}, {__index = compiler_env_warn})
  local function make_compiler_env(ast, scope, parent)
    local function _1_()
      return compiler.scopes.macro
    end
    local function _2_(symbol)
      compiler.assert(compiler.scopes.macro, "must call from macro", ast)
      return compiler.scopes.macro.manglings[tostring(symbol)]
    end
    local function _3_()
      return utils.sym(compiler.gensym((compiler.scopes.macro or scope)))
    end
    local function _4_(form)
      compiler.assert(compiler.scopes.macro, "must call from macro", ast)
      return compiler.macroexpand(form, compiler.scopes.macro)
    end
    local _6_
    do
      local _5_0 = utils.root.options
      if ((type(_5_0) == "table") and (nil ~= _5_0["compiler-env"])) then
        local compiler_env = _5_0["compiler-env"]
        _6_ = compiler_env
      else
        local _ = _5_0
        _6_ = safe_compiler_env
      end
    end
    return setmetatable({["assert-compile"] = compiler.assert, ["get-scope"] = _1_, ["in-scope?"] = _2_, ["list?"] = utils["list?"], ["multi-sym?"] = utils["multi-sym?"], ["sequence?"] = utils["sequence?"], ["sym?"] = utils["sym?"], ["table?"] = utils["table?"], ["varg?"] = utils["varg?"], _AST = ast, _CHUNK = parent, _IS_COMPILER = true, _SCOPE = scope, _SPECIALS = compiler.scopes.global.specials, _VARARG = utils.varg(), gensym = _3_, list = utils.list, macroexpand = _4_, sequence = utils.sequence, sym = utils.sym, unpack = unpack}, {__index = _6_})
  end
  local cfg = string.gmatch(package.config, "([^\n]+)")
  local dirsep, pathsep, pathmark = (cfg() or "/"), (cfg() or ";"), (cfg() or "?")
  local pkg_config = {dirsep = dirsep, pathmark = pathmark, pathsep = pathsep}
  local function escapepat(str)
    return string.gsub(str, "[^%w]", "%%%1")
  end
  local function search_module(modulename, pathstring)
    local pathsepesc = escapepat(pkg_config.pathsep)
    local pattern = ("([^%s]*)%s"):format(pathsepesc, pathsepesc)
    local no_dot_module = modulename:gsub("%.", pkg_config.dirsep)
    local fullpath = ((pathstring or utils["fennel-module"].path) .. pkg_config.pathsep)
    local function try_path(path)
      local filename = path:gsub(escapepat(pkg_config.pathmark), no_dot_module)
      local filename2 = path:gsub(escapepat(pkg_config.pathmark), modulename)
      local _1_0 = (io.open(filename) or io.open(filename2))
      if (nil ~= _1_0) then
        local file = _1_0
        file:close()
        return filename
      end
    end
    local function find_in_path(start)
      local _1_0 = fullpath:match(pattern, start)
      if (nil ~= _1_0) then
        local path = _1_0
        return (try_path(path) or find_in_path((start + #path + 1)))
      end
    end
    return find_in_path(1)
  end
  local function make_searcher(options)
    local opts = utils.copy(utils.root.options)
    for k, v in pairs((options or {})) do
      opts[k] = v
    end
    local function _1_(module_name)
      local filename = search_module(module_name)
      if filename then
        local function _2_(mod_name)
          return utils["fennel-module"].dofile(filename, opts, mod_name)
        end
        return _2_
      end
    end
    return _1_
  end
  local function macro_globals(env, globals)
    local allowed = current_global_names(env)
    for _, k in pairs((globals or {})) do
      table.insert(allowed, k)
    end
    return allowed
  end
  local function compiler_env_domodule(modname, env, _3fast)
    local filename = compiler.assert(search_module(modname), (modname .. " module not found."), _3fast)
    local globals = macro_globals(env, current_global_names())
    return utils["fennel-module"].dofile(filename, {allowedGlobals = globals, env = env, scope = compiler.scopes.compiler, useMetadata = utils.root.options.useMetadata})
  end
  local macro_loaded = {}
  local function metadata_only_fennel(modname)
    if ((modname == "fennel.macros") or (package and package.loaded and package.loaded[modname] and (package.loaded[modname].metadata == compiler.metadata))) then
      return {metadata = compiler.metadata}
    end
  end
  safe_compiler_env.require = function(modname)
    local function _1_()
      local mod = compiler_env_domodule(modname, safe_compiler_env)
      macro_loaded[modname] = mod
      return mod
    end
    return (macro_loaded[modname] or metadata_only_fennel(modname) or _1_())
  end
  local function add_macros(macros_2a, ast, scope)
    compiler.assert(utils["table?"](macros_2a), "expected macros to be table", ast)
    for k, v in pairs(macros_2a) do
      compiler.assert((type(v) == "function"), "expected each macro to be function", ast)
      scope.macros[k] = v
    end
    return nil
  end
  SPECIALS["require-macros"] = function(ast, scope, parent)
    compiler.assert((#ast == 2), "Expected one module name argument", ast)
    local modname = ast[2]
    if not macro_loaded[modname] then
      local env = make_compiler_env(ast, scope, parent)
      macro_loaded[modname] = compiler_env_domodule(modname, env, ast)
    end
    return add_macros(macro_loaded[modname], ast, scope, parent)
  end
  doc_special("require-macros", {"macro-module-name"}, "Load given module and use its contents as macro definitions in current scope.\nMacro module should return a table of macro functions with string keys.\nConsider using import-macros instead as it is more flexible.")
  local function emit_fennel(src, path, opts, sub_chunk)
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
        subopts = {nval = 1, tail = true}
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
      local function close_handlers_0_(ok_0_, ...)
        f:close()
        if ok_0_ then
          return ...
        else
          return error(..., 0)
        end
      end
      local function _1_()
        return f:read("*all"):gsub("[\13\n]*$", "")
      end
      src = close_handlers_0_(xpcall(_1_, (package.loaded.fennel or debug).traceback))
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
      emit_fennel(src, path, opts, sub_chunk)
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
    local modexpr = compiler.compile1(ast[2], scope, parent, {nval = 1})[1]
    if ((modexpr.type ~= "literal") or ((modexpr[1]):byte() ~= 34)) then
      if opts.fallback then
        return opts.fallback(modexpr)
      else
        return compiler.assert(false, "module name must be string literal", ast)
      end
    else
      local mod = load_code(("return " .. modexpr[1]))()
      local function _2_()
        local _1_0 = search_module(mod)
        if (nil ~= _1_0) then
          local fennel_path = _1_0
          return include_path(ast, opts, fennel_path, mod, true)
        else
          local _ = _1_0
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
      return (include_circular_fallback(mod, modexpr, opts.fallback, ast) or utils.root.scope.includes[mod] or _2_())
    end
  end
  doc_special("include", {"module-name-literal"}, "Like require but load the target module during compilation and embed it in the\nLua output. The module must be a string literal and resolvable at compile time.")
  local function eval_compiler_2a(ast, scope, parent)
    local env = make_compiler_env(ast, scope, parent)
    local opts = utils.copy(utils.root.options)
    opts.scope = compiler["make-scope"](compiler.scopes.compiler)
    opts.allowedGlobals = macro_globals(env, current_global_names())
    return load_code(compiler.compile(ast, opts), wrap_env(env))()
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
    ast[1] = old_first
    return val
  end
  doc_special("eval-compiler", {"..."}, "Evaluate the body at compile-time. Use the macro system instead if possible.")
  return {["current-global-names"] = current_global_names, ["load-code"] = load_code, ["macro-loaded"] = macro_loaded, ["make-compiler-env"] = make_compiler_env, ["make-searcher"] = make_searcher, ["search-module"] = search_module, ["wrap-env"] = wrap_env, doc = doc_2a}
end
package.preload["fennel.compiler"] = package.preload["fennel.compiler"] or function(...)
  local utils = require("fennel.utils")
  local parser = require("fennel.parser")
  local friend = require("fennel.friend")
  local unpack = (_G.unpack or table.unpack)
  local scopes = {}
  local function make_scope(parent)
    local parent0 = (parent or scopes.global)
    local _0_
    if parent0 then
      _0_ = ((parent0.depth or 0) + 1)
    else
      _0_ = 0
    end
    return {autogensyms = {}, depth = _0_, hashfn = (parent0 and parent0.hashfn), includes = setmetatable({}, {__index = (parent0 and parent0.includes)}), macros = setmetatable({}, {__index = (parent0 and parent0.macros)}), manglings = setmetatable({}, {__index = (parent0 and parent0.manglings)}), parent = parent0, refedglobals = setmetatable({}, {__index = (parent0 and parent0.refedglobals)}), specials = setmetatable({}, {__index = (parent0 and parent0.specials)}), symmeta = setmetatable({}, {__index = (parent0 and parent0.symmeta)}), unmanglings = setmetatable({}, {__index = (parent0 and parent0.unmanglings)}), vararg = (parent0 and parent0.vararg)}
  end
  local function assert_compile(condition, msg, ast)
    if not condition then
      local _0_ = (utils.root.options or {})
      local source = _0_["source"]
      local unfriendly = _0_["unfriendly"]
      utils.root.reset()
      if unfriendly then
        local m = getmetatable(ast)
        local filename = ((m and m.filename) or ast.filename or "unknown")
        local line = ((m and m.line) or ast.line or "?")
        local target = nil
        local function _1_()
          if utils["sym?"](ast[1]) then
            return utils.deref(ast[1])
          else
            return (ast[1] or "()")
          end
        end
        target = tostring(_1_())
        error(string.format("Compile error in '%s' %s:%s: %s", target, filename, line, msg), 0)
      else
        friend["assert-compile"](condition, msg, ast, source)
      end
    end
    return condition
  end
  scopes.global = make_scope()
  scopes.global.vararg = true
  scopes.compiler = make_scope(scopes.global)
  scopes.macro = scopes.global
  local serialize_subst = {["\11"] = "\\v", ["\12"] = "\\f", ["\7"] = "\\a", ["\8"] = "\\b", ["\9"] = "\\t", ["\n"] = "n"}
  local function serialize_string(str)
    local function _0_(_241)
      return ("\\" .. _241:byte())
    end
    return string.gsub(string.gsub(string.format("%q", str), ".", serialize_subst), "[\128-\255]", _0_)
  end
  local function global_mangling(str)
    if utils["valid-lua-identifier?"](str) then
      return str
    else
      local function _0_(_241)
        return string.format("_%02x", _241:byte())
      end
      return ("__fnl_global__" .. str:gsub("[^%w]", _0_))
    end
  end
  local function global_unmangling(identifier)
    local _0_0 = string.match(identifier, "^__fnl_global__(.*)$")
    if (nil ~= _0_0) then
      local rest = _0_0
      local _1_0 = nil
      local function _2_(_241)
        return string.char(tonumber(_241:sub(2), 16))
      end
      _1_0 = string.gsub(rest, "_[%da-f][%da-f]", _2_)
      return _1_0
    else
      local _ = _0_0
      return identifier
    end
  end
  local allowed_globals = nil
  local function global_allowed(name)
    local found_3f = not allowed_globals
    if not allowed_globals then
      return true
    else
      return utils["member?"](name, allowed_globals)
    end
  end
  local function unique_mangling(original, mangling, scope, append)
    if scope.unmanglings[mangling] then
      return unique_mangling(original, (original .. append), scope, (append + 1))
    else
      return mangling
    end
  end
  local function local_mangling(str, scope, ast, temp_manglings)
    assert_compile(not utils["multi-sym?"](str), ("unexpected multi symbol " .. str), ast)
    local append = 0
    local raw = nil
    if (utils["lua-keywords"][str] or str:match("^%d")) then
      raw = ("_" .. str)
    else
      raw = str
    end
    local mangling = nil
    local function _1_(_241)
      return string.format("_%02x", _241:byte())
    end
    mangling = string.gsub(string.gsub(raw, "-", "_"), "[^%w_]", _1_)
    local unique = unique_mangling(mangling, mangling, scope, 0)
    scope.unmanglings[unique] = str
    do
      local manglings = (temp_manglings or scope.manglings)
      manglings[str] = unique
    end
    return unique
  end
  local function apply_manglings(scope, new_manglings, ast)
    for raw, mangled in pairs(new_manglings) do
      assert_compile(not scope.refedglobals[mangled], ("use of global " .. raw .. " is aliased by a local"), ast)
      scope.manglings[raw] = mangled
    end
    return nil
  end
  local function combine_parts(parts, scope)
    local ret = (scope.manglings[parts[1]] or global_mangling(parts[1]))
    for i = 2, #parts, 1 do
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
  local function gensym(scope, base)
    local append, mangling = 0, ((base or "") .. "_0_")
    while scope.unmanglings[mangling] do
      mangling = ((base or "") .. "_" .. append .. "_")
      append = (append + 1)
    end
    scope.unmanglings[mangling] = true
    return mangling
  end
  local function autogensym(base, scope)
    local _0_0 = utils["multi-sym?"](base)
    if (nil ~= _0_0) then
      local parts = _0_0
      parts[1] = autogensym(parts[1], scope)
      return table.concat(parts, ((parts["multi-sym-method-call"] and ":") or "."))
    else
      local _ = _0_0
      local function _1_()
        local mangling = gensym(scope, base:sub(1, ( - 2)))
        scope.autogensyms[base] = mangling
        return mangling
      end
      return (scope.autogensyms[base] or _1_())
    end
  end
  local function check_binding_valid(symbol, scope, ast)
    local name = utils.deref(symbol)
    assert_compile(not (scope.specials[name] or scope.macros[name]), ("local %s was overshadowed by a special form or macro"):format(name), ast)
    return assert_compile(not utils["quoted?"](symbol), string.format("macro tried to bind %s without gensym", name), symbol)
  end
  local function declare_local(symbol, meta, scope, ast, temp_manglings)
    check_binding_valid(symbol, scope, ast)
    local name = utils.deref(symbol)
    assert_compile(not utils["multi-sym?"](name), ("unexpected multi symbol " .. name), ast)
    scope.symmeta[name] = meta
    return local_mangling(name, scope, ast, temp_manglings)
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
  local function symbol_to_expression(symbol, scope, reference_3f)
    utils.hook("symbol-to-expression", symbol, scope, reference_3f)
    local name = symbol[1]
    local multi_sym_parts = utils["multi-sym?"](name)
    local name0 = (hashfn_arg_name(name, multi_sym_parts, scope) or name)
    local parts = (multi_sym_parts or {name0})
    local etype = (((#parts > 1) and "expression") or "sym")
    local local_3f = scope.manglings[parts[1]]
    if (local_3f and scope.symmeta[parts[1]]) then
      scope.symmeta[parts[1]]["used"] = true
    end
    assert_compile((not reference_3f or local_3f or global_allowed(parts[1])), ("unknown global in strict mode: " .. parts[1]), symbol)
    if (allowed_globals and not local_3f) then
      utils.root.scope.refedglobals[parts[1]] = true
    end
    return utils.expr(combine_parts(parts, scope), etype)
  end
  local function emit(chunk, out, ast)
    if (type(out) == "table") then
      return table.insert(chunk, out)
    else
      return table.insert(chunk, {ast = ast, leaf = out})
    end
  end
  local function peephole(chunk)
    if chunk.leaf then
      return chunk
    elseif ((#chunk >= 3) and (chunk[(#chunk - 2)].leaf == "do") and not chunk[(#chunk - 1)].leaf and (chunk[#chunk].leaf == "end")) then
      local kid = peephole(chunk[(#chunk - 1)])
      local new_chunk = {ast = chunk.ast}
      for i = 1, (#chunk - 3), 1 do
        table.insert(new_chunk, peephole(chunk[i]))
      end
      for i = 1, #kid, 1 do
        table.insert(new_chunk, kid[i])
      end
      return new_chunk
    else
      return utils.map(chunk, peephole)
    end
  end
  local function flatten_chunk_correlated(main_chunk)
    local function flatten(chunk, out, last_line, file)
      local last_line0 = last_line
      if chunk.leaf then
        out[last_line0] = ((out[last_line0] or "") .. " " .. chunk.leaf)
      else
        for _, subchunk in ipairs(chunk) do
          if (subchunk.leaf or (#subchunk > 0)) then
            if (subchunk.ast and (file == subchunk.ast.file)) then
              last_line0 = math.max(last_line0, (subchunk.ast.line or 0))
            end
            last_line0 = flatten(subchunk, out, last_line0, file)
          end
        end
      end
      return last_line0
    end
    local out = {}
    local last = flatten(main_chunk, out, 1, main_chunk.file)
    for i = 1, last do
      if (out[i] == nil) then
        out[i] = ""
      end
    end
    return table.concat(out, "\n")
  end
  local function flatten_chunk(sm, chunk, tab, depth)
    if chunk.leaf then
      local code = chunk.leaf
      local info = chunk.ast
      if sm then
        sm[(#sm + 1)] = ((info and info.line) or ( - 1))
      end
      return code
    else
      local tab0 = nil
      do
        local _0_0 = tab
        if (_0_0 == true) then
          tab0 = "  "
        elseif (_0_0 == false) then
          tab0 = ""
        elseif (_0_0 == tab) then
          tab0 = tab
        elseif (_0_0 == nil) then
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
        end
      end
      return table.concat(utils.map(chunk, parter), "\n")
    end
  end
  local fennel_sourcemap = {}
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
      return flatten_chunk_correlated(chunk0), {}
    else
      local sm = {}
      local ret = flatten_chunk(sm, chunk0, options.indent, 0)
      if sm then
        sm.short_src = (options.filename or ret)
        if options.filename then
          sm.key = ("@" .. options.filename)
        else
          sm.key = ret
        end
        fennel_sourcemap[sm.key] = sm
      end
      return ret, sm
    end
  end
  local function make_metadata()
    local function _0_(self, tgt, key)
      if self[tgt] then
        return self[tgt][key]
      end
    end
    local function _1_(self, tgt, key, value)
      self[tgt] = (self[tgt] or {})
      self[tgt][key] = value
      return tgt
    end
    local function _2_(self, tgt, ...)
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
    return setmetatable({}, {__index = {get = _0_, set = _1_, setall = _2_}, __mode = "k"})
  end
  local function exprs1(exprs)
    return table.concat(utils.map(exprs, 1), ", ")
  end
  local function keep_side_effects(exprs, chunk, start, ast)
    local start0 = (start or 1)
    for j = start0, #exprs, 1 do
      local se = exprs[j]
      if ((se.type == "expression") and (se[1] ~= "nil")) then
        emit(chunk, string.format("do local _ = %s end", tostring(se)), ast)
      elseif (se.type == "statement") then
        local code = tostring(se)
        emit(chunk, (((code:byte() == 40) and ("do end " .. code)) or code), ast)
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
          for i = (n + 1), len, 1 do
            exprs[i] = nil
          end
        else
          for i = (#exprs + 1), n, 1 do
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
      local function _2_()
        if (result == "") then
          return "nil"
        else
          return result
        end
      end
      emit(parent, string.format("%s = %s", opts.target, _2_()), ast)
    end
    if (opts.tail or opts.target) then
      return {returned = true}
    else
      local _3_0 = exprs
      _3_0["returned"] = true
      return _3_0
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
    local macro_2a = (utils["sym?"](ast[1]) and scope.macros[utils.deref(ast[1])])
    if (not macro_2a and multi_sym_parts) then
      local nested_macro = find_in_table(scope.macros, 1)
      assert_compile((not scope.macros[multi_sym_parts[1]] or (type(nested_macro) == "function")), "macro not found in imported macro module", ast)
      return nested_macro
    else
      return macro_2a
    end
  end
  local function macroexpand_2a(ast, scope, once)
    if not utils["list?"](ast) then
      return ast
    else
      local macro_2a = find_macro(ast, scope, utils["multi-sym?"](ast[1]))
      if not macro_2a then
        return ast
      else
        local old_scope = scopes.macro
        local _ = nil
        scopes.macro = scope
        _ = nil
        local ok, transformed = pcall(macro_2a, unpack(ast, 2))
        scopes.macro = old_scope
        assert_compile(ok, transformed, ast)
        if (once or not transformed) then
          return transformed
        else
          return macroexpand_2a(transformed, scope)
        end
      end
    end
  end
  local function compile_special(ast, scope, parent, opts, special)
    local exprs = (special(ast, scope, parent, opts) or utils.expr("nil", "literal"))
    local exprs0 = nil
    if (type(exprs) == "string") then
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
  local function compile_call(ast, scope, parent, opts, compile1)
    utils.hook("call", ast, scope)
    local len = #ast
    local first = ast[1]
    local multi_sym_parts = utils["multi-sym?"](first)
    local special = (utils["sym?"](first) and scope.specials[utils.deref(first)])
    assert_compile((len > 0), "expected a function, macro, or special to call", ast)
    if special then
      return compile_special(ast, scope, parent, opts, special)
    elseif (multi_sym_parts and multi_sym_parts["multi-sym-method-call"]) then
      local table_with_method = table.concat({unpack(multi_sym_parts, 1, (#multi_sym_parts - 1))}, ".")
      local method_to_call = multi_sym_parts[#multi_sym_parts]
      local new_ast = utils.list(utils.sym(":", scope), utils.sym(table_with_method, scope), method_to_call)
      for i = 2, len, 1 do
        new_ast[(#new_ast + 1)] = ast[i]
      end
      return compile1(new_ast, scope, parent, opts)
    else
      local fargs = {}
      local fcallee = compile1(ast[1], scope, parent, {nval = 1})[1]
      assert_compile((fcallee.type ~= "literal"), ("cannot call literal value " .. tostring(first)), ast)
      for i = 2, len, 1 do
        local subexprs = compile1(ast[i], scope, parent, {nval = (((i ~= len) and 1) or nil)})
        fargs[(#fargs + 1)] = (subexprs[1] or utils.expr("nil", "literal"))
        if (i == len) then
          for j = 2, #subexprs, 1 do
            fargs[(#fargs + 1)] = subexprs[j]
          end
        else
          keep_side_effects(subexprs, parent, 2, ast[i])
        end
      end
      local call = string.format("%s(%s)", tostring(fcallee), exprs1(fargs))
      return handle_compile_opts({utils.expr(call, "statement")}, parent, opts, ast)
    end
  end
  local function compile_varg(ast, scope, parent, opts)
    assert_compile(scope.vararg, "unexpected vararg", ast)
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
  local function compile_scalar(ast, scope, parent, opts)
    local serialize = nil
    do
      local _0_0 = type(ast)
      if (_0_0 == "nil") then
        serialize = tostring
      elseif (_0_0 == "boolean") then
        serialize = tostring
      elseif (_0_0 == "string") then
        serialize = serialize_string
      elseif (_0_0 == "number") then
        local function _1_(...)
          return string.format("%.17g", ...)
        end
        serialize = _1_
      else
      serialize = nil
      end
    end
    return handle_compile_opts({utils.expr(serialize(ast), "literal")}, parent, opts)
  end
  local function compile_table(ast, scope, parent, opts, compile1)
    local buffer = {}
    for i = 1, #ast, 1 do
      local nval = ((i ~= #ast) and 1)
      buffer[(#buffer + 1)] = exprs1(compile1(ast[i], scope, parent, {nval = nval}))
    end
    local function write_other_values(k)
      if ((type(k) ~= "number") or (math.floor(k) ~= k) or (k < 1) or (k > #ast)) then
        if ((type(k) == "string") and utils["valid-lua-identifier?"](k)) then
          return {k, k}
        else
          local _0_ = compile1(k, scope, parent, {nval = 1})
          local compiled = _0_[1]
          local kstr = ("[" .. tostring(compiled) .. "]")
          return {kstr, k}
        end
      end
    end
    do
      local keys = nil
      do
        local _0_0 = utils.kvmap(ast, write_other_values)
        local function _1_(a, b)
          return (a[1] < b[1])
        end
        table.sort(_0_0, _1_)
        keys = _0_0
      end
      local function _1_(k)
        local v = tostring(compile1(ast[k[2]], scope, parent, {nval = 1})[1])
        return string.format("%s = %s", k[1], v)
      end
      utils.map(keys, _1_, buffer)
    end
    return handle_compile_opts({utils.expr(("{" .. table.concat(buffer, ", ") .. "}"), "expression")}, parent, opts, ast)
  end
  local function compile1(ast, scope, parent, opts)
    local opts0 = (opts or {})
    local ast0 = macroexpand_2a(ast, scope)
    if utils["list?"](ast0) then
      return compile_call(ast0, scope, parent, opts0, compile1)
    elseif utils["varg?"](ast0) then
      return compile_varg(ast0, scope, parent, opts0)
    elseif utils["sym?"](ast0) then
      return compile_sym(ast0, scope, parent, opts0)
    elseif (type(ast0) == "table") then
      return compile_table(ast0, scope, parent, opts0, compile1)
    elseif ((type(ast0) == "nil") or (type(ast0) == "boolean") or (type(ast0) == "number") or (type(ast0) == "string")) then
      return compile_scalar(ast0, scope, parent, opts0)
    else
      return assert_compile(false, ("could not compile value of type " .. type(ast0)), ast0)
    end
  end
  local function destructure(to, from, ast, scope, parent, opts)
    local opts0 = (opts or {})
    local _0_ = opts0
    local declaration = _0_["declaration"]
    local forceglobal = _0_["forceglobal"]
    local forceset = _0_["forceset"]
    local isvar = _0_["isvar"]
    local nomulti = _0_["nomulti"]
    local noundef = _0_["noundef"]
    local setter = nil
    if declaration then
      setter = "local %s = %s"
    else
      setter = "%s = %s"
    end
    local new_manglings = {}
    local function getname(symbol, up1)
      local raw = symbol[1]
      assert_compile(not (nomulti and utils["multi-sym?"](raw)), ("unexpected multi symbol " .. raw), up1)
      if declaration then
        return declare_local(symbol, {var = isvar}, scope, symbol, new_manglings)
      else
        local parts = (utils["multi-sym?"](raw) or {raw})
        local meta = scope.symmeta[parts[1]]
        if ((#parts == 1) and not forceset) then
          assert_compile(not (forceglobal and meta), string.format("global %s conflicts with local", tostring(symbol)), symbol)
          assert_compile(not (meta and not meta.var), ("expected var " .. raw), symbol)
          assert_compile((meta or not noundef), ("expected local " .. parts[1]), symbol)
        end
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
      local function _2_(_241)
        if scope.manglings[_241] then
          return _241
        else
          return "nil"
        end
      end
      inits = utils.map(lvalues, _2_)
      local init = table.concat(inits, ", ")
      local lvalue = table.concat(lvalues, ", ")
      local plen, plast = #parent, parent[#parent]
      local ret = compile1(from, scope, parent, {target = lvalue})
      if declaration then
        for pi = plen, #parent do
          if (parent[pi] == plast) then
            plen = pi
          end
        end
        if ((#parent == (plen + 1)) and parent[#parent].leaf) then
          parent[#parent]["leaf"] = ("local " .. parent[#parent].leaf)
        else
          table.insert(parent, (plen + 1), {ast = ast, leaf = ("local " .. lvalue .. " = " .. init)})
        end
      end
      return ret
    end
    local function destructure1(left, rightexprs, up1, top)
      if (utils["sym?"](left) and (left[1] ~= "nil")) then
        local lname = getname(left, up1)
        check_binding_valid(left, scope, left)
        if top then
          compile_top_target({lname})
        else
          emit(parent, setter:format(lname, exprs1(rightexprs)), left)
        end
      elseif utils["table?"](left) then
        local s = gensym(scope)
        local right = nil
        if top then
          right = exprs1(compile1(from, scope, parent))
        else
          right = exprs1(rightexprs)
        end
        if (right == "") then
          right = "nil"
        end
        emit(parent, string.format("local %s = %s", s, right), left)
        for k, v in utils.stablepairs(left) do
          if (utils["sym?"](left[k]) and (left[k][1] == "&")) then
            assert_compile(((type(k) == "number") and not left[(k + 2)]), "expected rest argument before last parameter", left)
            local unpack_str = "{(table.unpack or unpack)(%s, %s)}"
            local formatted = string.format(unpack_str, s, k)
            local subexpr = utils.expr(formatted, "expression")
            destructure1(left[(k + 1)], {subexpr}, left)
            return
          else
            if (utils["sym?"](k) and (tostring(k) == ":") and utils["sym?"](v)) then
              k = tostring(v)
            end
            if (type(k) ~= "number") then
              k = serialize_string(k)
            end
            local subexpr = utils.expr(string.format("%s[%s]", s, k), "expression")
            destructure1(v, {subexpr}, left)
          end
        end
      elseif utils["list?"](left) then
        local left_names, tables = {}, {}
        for i, name in ipairs(left) do
          local symname = nil
          if utils["sym?"](name) then
            symname = getname(name, up1)
          else
            symname = gensym(scope)
            tables[i] = {name, utils.expr(symname, "sym")}
          end
          table.insert(left_names, symname)
        end
        if top then
          compile_top_target(left_names)
        else
          local lvalue = table.concat(left_names, ", ")
          local setting = setter:format(lvalue, exprs1(rightexprs))
          emit(parent, setting, left)
        end
        for _, pair in utils.stablepairs(tables) do
          destructure1(pair[1], {pair[2]}, left)
        end
      else
        assert_compile(false, string.format("unable to bind %s %s", type(left), tostring(left)), (((type(up1[2]) == "table") and up1[2]) or up1))
      end
      if top then
        return {returned = true}
      end
    end
    local ret = destructure1(to, nil, ast, true)
    utils.hook("destructure", from, to, scope)
    apply_manglings(scope, new_manglings, ast)
    return ret
  end
  local function require_include(ast, scope, parent, opts)
    opts.fallback = function(e)
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
    local _0_ = utils.root
    _0_["set-reset"](_0_)
    allowed_globals = opts.allowedGlobals
    if (opts.indent == nil) then
      opts.indent = "  "
    end
    if opts.requireAsInclude then
      scope.specials.require = require_include
    end
    utils.root.chunk, utils.root.scope, utils.root.options = chunk, scope, opts
    for ok, val in parser.parser(strm, opts.filename, opts) do
      vals[(#vals + 1)] = val
    end
    for i = 1, #vals, 1 do
      local exprs = compile1(vals[i], scope, chunk, {nval = (((i < #vals) and 0) or nil), tail = (i == #vals)})
      keep_side_effects(exprs, chunk, nil, vals[i])
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
    local _0_ = utils.root
    _0_["set-reset"](_0_)
    allowed_globals = opts0.allowedGlobals
    if (opts0.indent == nil) then
      opts0.indent = "  "
    end
    if opts0.requireAsInclude then
      scope.specials.require = require_include
    end
    utils.root.chunk, utils.root.scope, utils.root.options = chunk, scope, opts0
    local exprs = compile1(ast, scope, chunk, {tail = true})
    keep_side_effects(exprs, chunk, nil, ast)
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
      local remap = fennel_sourcemap[info.source]
      if (remap and remap[info.currentline]) then
        info["short-src"] = remap["short-src"]
        info.currentline = remap[info.currentline]
      end
      if (info.what == "Lua") then
        local function _1_()
          if info.name then
            return ("'" .. info.name .. "'")
          else
            return "?"
          end
        end
        return string.format("  %s:%d: in function %s", info.short_src, info.currentline, _1_())
      elseif (info["short-src"] == "(tail call)") then
        return "  (tail call)"
      else
        return string.format("  %s:%d: in main chunk", info.short_src, info.currentline)
      end
    end
  end
  local function traceback(msg, start)
    local msg0 = (msg or "")
    if ((msg0:find("^Compile error") or msg0:find("^Parse error")) and not utils["debug-on?"]("trace")) then
      return msg0
    else
      local lines = {}
      if (msg0:find("^Compile error") or msg0:find("^Parse error")) then
        table.insert(lines, msg0)
      else
        local newmsg = msg0:gsub("^[^:]*:%d+:%s+", "runtime error: ")
        table.insert(lines, newmsg)
      end
      table.insert(lines, "stack traceback:")
      local done_3f, level = false, (start or 2)
      while not done_3f do
        do
          local _1_0 = debug.getinfo(level, "Sln")
          if (_1_0 == nil) then
            done_3f = true
          elseif (nil ~= _1_0) then
            local info = _1_0
            table.insert(lines, traceback_frame(info))
          end
        end
        level = (level + 1)
      end
      return table.concat(lines, "\n")
    end
  end
  local function entry_transform(fk, fv)
    local function _0_(k, v)
      if (type(k) == "number") then
        return k, fv(v)
      else
        return fk(k), fv(v)
      end
    end
    return _0_
  end
  local function no()
    return nil
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
    local function q(x)
      return do_quote(x, scope, parent, runtime_3f)
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
      local symstr = utils.deref(form)
      assert_compile(not runtime_3f, "symbols may only be used at compile time", form)
      if (symstr:find("#$") or symstr:find("#[:.]")) then
        return string.format("sym('%s', nil, {filename=%s, line=%s})", autogensym(symstr, scope), filename, (form.line or "nil"))
      else
        return string.format("sym('%s', nil, {quoted=true, filename=%s, line=%s})", symstr, filename, (form.line or "nil"))
      end
    elseif (utils["list?"](form) and utils["sym?"](form[1]) and (utils.deref(form[1]) == "unquote")) then
      local payload = form[2]
      local res = unpack(compile1(payload, scope, parent))
      return res[1]
    elseif utils["list?"](form) then
      local mapped = utils.kvmap(form, entry_transform(no, q))
      local filename = nil
      if form.filename then
        filename = string.format("%q", form.filename)
      else
        filename = "nil"
      end
      assert_compile(not runtime_3f, "lists may only be used at compile time", form)
      return string.format(("setmetatable({filename=%s, line=%s, bytestart=%s, %s}" .. ", getmetatable(list()))"), filename, (form.line or "nil"), (form.bytestart or "nil"), mixed_concat(mapped, ", "))
    elseif (type(form) == "table") then
      local mapped = utils.kvmap(form, entry_transform(q, q))
      local source = getmetatable(form)
      local filename = nil
      if source.filename then
        filename = string.format("%q", source.filename)
      else
        filename = "nil"
      end
      local function _1_()
        if source then
          return source.line
        else
          return "nil"
        end
      end
      return string.format("setmetatable({%s}, {filename=%s, line=%s})", mixed_concat(mapped, ", "), filename, _1_())
    elseif (type(form) == "string") then
      return serialize_string(form)
    else
      return tostring(form)
    end
  end
  return {["apply-manglings"] = apply_manglings, ["compile-stream"] = compile_stream, ["compile-string"] = compile_string, ["declare-local"] = declare_local, ["do-quote"] = do_quote, ["global-mangling"] = global_mangling, ["global-unmangling"] = global_unmangling, ["keep-side-effects"] = keep_side_effects, ["make-scope"] = make_scope, ["require-include"] = require_include, ["symbol-to-expression"] = symbol_to_expression, assert = assert_compile, autogensym = autogensym, compile = compile, compile1 = compile1, destructure = destructure, emit = emit, gensym = gensym, macroexpand = macroexpand_2a, metadata = make_metadata(), scopes = scopes, traceback = traceback}
end
package.preload["fennel.friend"] = package.preload["fennel.friend"] or function(...)
  local function ast_source(ast)
    local m = getmetatable(ast)
    if (m and m.line and m) then
      return m
    else
      return ast
    end
  end
  local suggestions = {["$ and $... in hashfn are mutually exclusive"] = {"modifying the hashfn so it only contains $... or $, $1, $2, $3, etc"}, ["can't start multisym segment with a digit"] = {"removing the digit", "adding a non-digit before the digit"}, ["cannot call literal value"] = {"checking for typos", "checking for a missing function name"}, ["could not compile value of type "] = {"debugging the macro you're calling not to return a coroutine or userdata"}, ["could not read number (.*)"] = {"removing the non-digit character", "beginning the identifier with a non-digit if it is not meant to be a number"}, ["expected a function.* to call"] = {"removing the empty parentheses", "using square brackets if you want an empty table"}, ["expected binding table"] = {"placing a table here in square brackets containing identifiers to bind"}, ["expected body expression"] = {"putting some code in the body of this form after the bindings"}, ["expected each macro to be function"] = {"ensuring that the value for each key in your macros table contains a function", "avoid defining nested macro tables"}, ["expected even number of name/value bindings"] = {"finding where the identifier or value is missing"}, ["expected even number of values in table literal"] = {"removing a key", "adding a value"}, ["expected local"] = {"looking for a typo", "looking for a local which is used out of its scope"}, ["expected macros to be table"] = {"ensuring your macro definitions return a table"}, ["expected parameters"] = {"adding function parameters as a list of identifiers in brackets"}, ["expected rest argument before last parameter"] = {"moving & to right before the final identifier when destructuring"}, ["expected symbol for function parameter: (.*)"] = {"changing %s to an identifier instead of a literal value"}, ["expected var (.*)"] = {"declaring %s using var instead of let/local", "introducing a new local instead of changing the value of %s"}, ["expected vararg as last parameter"] = {"moving the \"...\" to the end of the parameter list"}, ["expected whitespace before opening delimiter"] = {"adding whitespace"}, ["global (.*) conflicts with local"] = {"renaming local %s"}, ["illegal character: (.)"] = {"deleting or replacing %s", "avoiding reserved characters like \", \\, ', ~, ;, @, `, and comma"}, ["local (.*) was overshadowed by a special form or macro"] = {"renaming local %s"}, ["macro not found in macro module"] = {"checking the keys of the imported macro module's returned table"}, ["macro tried to bind (.*) without gensym"] = {"changing to %s# when introducing identifiers inside macros"}, ["malformed multisym"] = {"ensuring each period or colon is not followed by another period or colon"}, ["may only be used at compile time"] = {"moving this to inside a macro if you need to manipulate symbols/lists", "using square brackets instead of parens to construct a table"}, ["method must be last component"] = {"using a period instead of a colon for field access", "removing segments after the colon", "making the method call, then looking up the field on the result"}, ["mismatched closing delimiter (.), expected (.)"] = {"replacing %s with %s", "deleting %s", "adding matching opening delimiter earlier"}, ["multisym method calls may only be in call position"] = {"using a period instead of a colon to reference a table's fields", "putting parens around this"}, ["unable to bind (.*)"] = {"replacing the %s with an identifier"}, ["unexpected closing delimiter (.)"] = {"deleting %s", "adding matching opening delimiter earlier"}, ["unexpected multi symbol (.*)"] = {"removing periods or colons from %s"}, ["unexpected vararg"] = {"putting \"...\" at the end of the fn parameters if the vararg was intended"}, ["unknown global in strict mode: (.*)"] = {"looking to see if there's a typo", "using the _G table instead, eg. _G.%s if you really want a global", "moving this code to somewhere that %s is in scope", "binding %s as a local in the scope of this code"}, ["unused local (.*)"] = {"fixing a typo so %s is used", "renaming the local to _%s"}, ["use of global (.*) is aliased by a local"] = {"renaming local %s", "refer to the global using _G.%s instead of directly"}}
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
    f:close()
    return codeline, bytes
  end
  local function read_line_from_source(source, line)
    local lines, bytes, codeline = 0, 0
    for this_line, newline in string.gmatch((source .. "\n"), "(.-)(\13?\n)") do
      lines = (lines + 1)
      if (lines == line) then
        codeline = this_line
        break
      end
      bytes = (bytes + #newline + #this_line)
    end
    return codeline, bytes
  end
  local function read_line(filename, line, source)
    if source then
      return read_line_from_source(source, line)
    else
      return read_line_from_file(filename, line)
    end
  end
  local function friendly_msg(msg, _0_0, source)
    local _1_ = _0_0
    local byteend = _1_["byteend"]
    local bytestart = _1_["bytestart"]
    local filename = _1_["filename"]
    local line = _1_["line"]
    local ok, codeline, bol, eol = pcall(read_line, filename, line, source)
    local suggestions0 = suggest(msg)
    local out = {msg, ""}
    if (ok and codeline) then
      table.insert(out, codeline)
    end
    if (ok and codeline and bytestart and byteend) then
      table.insert(out, (string.rep(" ", (bytestart - bol - 1)) .. "^" .. string.rep("^", math.min((byteend - bytestart), ((bol + #codeline) - bytestart)))))
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
  local function assert_compile(condition, msg, ast, source)
    if not condition then
      local _1_ = ast_source(ast)
      local filename = _1_["filename"]
      local line = _1_["line"]
      error(friendly_msg(("Compile error in %s:%s\n  %s"):format((filename or "unknown"), (line or "?"), msg), ast_source(ast), source), 0)
    end
    return condition
  end
  local function parse_error(msg, filename, line, bytestart, source)
    return error(friendly_msg(("Parse error in %s:%s\n  %s"):format(filename, line, msg), {bytestart = bytestart, filename = filename, line = line}, source), 0)
  end
  return {["assert-compile"] = assert_compile, ["parse-error"] = parse_error}
end
package.preload["fennel.parser"] = package.preload["fennel.parser"] or function(...)
  local utils = require("fennel.utils")
  local friend = require("fennel.friend")
  local unpack = (_G.unpack or table.unpack)
  local function granulate(getchunk)
    local c, index, done_3f = "", 1, false
    local function _0_(parser_state)
      if not done_3f then
        if (index <= #c) then
          local b = c:byte(index)
          index = (index + 1)
          return b
        else
          c = getchunk(parser_state)
          if (not c or (c == "")) then
            done_3f = true
            return nil
          end
          index = 2
          return c:byte(1)
        end
      end
    end
    local function _1_()
      c = ""
      return nil
    end
    return _0_, _1_
  end
  local function string_stream(str)
    local str0 = str:gsub("^#![^\n]*\n", "")
    local index = 1
    local function _0_()
      local r = str0:byte(index)
      index = (index + 1)
      return r
    end
    return _0_
  end
  local delims = {[123] = 125, [125] = true, [40] = 41, [41] = true, [91] = 93, [93] = true}
  local function whitespace_3f(b)
    return ((b == 32) or ((b >= 9) and (b <= 13)))
  end
  local function symbolchar_3f(b)
    return ((b > 32) and not delims[b] and (b ~= 127) and (b ~= 34) and (b ~= 39) and (b ~= 126) and (b ~= 59) and (b ~= 44) and (b ~= 64) and (b ~= 96))
  end
  local prefixes = {[35] = "hashfn", [39] = "quote", [44] = "unquote", [96] = "quote"}
  local function parser(getbyte, filename, options)
    local stack = {}
    local line = 1
    local byteindex = 0
    local lastb = nil
    local function ungetb(ub)
      if (ub == 10) then
        line = (line - 1)
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
      end
      return r
    end
    local function parse_error(msg)
      local _0_ = (utils.root.options or {})
      local source = _0_["source"]
      local unfriendly = _0_["unfriendly"]
      utils.root.reset()
      if unfriendly then
        return error(string.format("Parse error in %s:%s: %s", (filename or "unknown"), (line or "?"), msg), 0)
      else
        return friend["parse-error"](msg, (filename or "unknown"), (line or "?"), byteindex, source)
      end
    end
    local function parse_stream()
      local whitespace_since_dispatch, done_3f, retval = true
      local function dispatch(v)
        if (#stack == 0) then
          retval, done_3f, whitespace_since_dispatch = v, true, false
          return nil
        elseif stack[#stack].prefix then
          local stacktop = stack[#stack]
          stack[#stack] = nil
          return dispatch(utils.list(utils.sym(stacktop.prefix), v))
        else
          whitespace_since_dispatch = false
          return table.insert(stack[#stack], v)
        end
      end
      local function badend()
        local accum = utils.map(stack, "closer")
        return parse_error(string.format("expected closing delimiter%s %s", (((#stack == 1) and "") or "s"), string.char(unpack(accum))))
      end
      while true do
        local b = nil
        while true do
          b = getb()
          if (b and whitespace_3f(b)) then
            whitespace_since_dispatch = true
          end
          if (not b or not whitespace_3f(b)) then
            break
          end
        end
        if not b then
          if (#stack > 0) then
            badend()
          end
          return nil
        end
        if (b == 59) then
          while true do
            b = getb()
            if (not b or (b == 10)) then
              break
            end
          end
        elseif (type(delims[b]) == "number") then
          if not whitespace_since_dispatch then
            parse_error(("expected whitespace before opening delimiter " .. string.char(b)))
          end
          table.insert(stack, setmetatable({bytestart = byteindex, closer = delims[b], filename = filename, line = line}, getmetatable(utils.list())))
        elseif delims[b] then
          local last = stack[#stack]
          if (#stack == 0) then
            parse_error(("unexpected closing delimiter " .. string.char(b)))
          end
          local val = nil
          if (last.closer ~= b) then
            parse_error(("mismatched closing delimiter " .. string.char(b) .. ", expected " .. string.char(last.closer)))
          end
          last.byteend = byteindex
          if (b == 41) then
            val = last
          elseif (b == 93) then
            val = utils.sequence(unpack(last))
            for k, v in pairs(last) do
              getmetatable(val)[k] = v
            end
          else
            if ((#last % 2) ~= 0) then
              byteindex = (byteindex - 1)
              parse_error("expected even number of values in table literal")
            end
            val = {}
            setmetatable(val, last)
            for i = 1, #last, 2 do
              if ((tostring(last[i]) == ":") and utils["sym?"](last[(i + 1)]) and utils["sym?"](last[i])) then
                last[i] = tostring(last[(i + 1)])
              end
              val[last[i]] = last[(i + 1)]
            end
          end
          stack[#stack] = nil
          dispatch(val)
        elseif (b == 34) then
          local chars = {34}
          local state = "base"
          stack[(#stack + 1)] = {closer = 34}
          while true do
            b = getb()
            chars[(#chars + 1)] = b
            if (state == "base") then
              if (b == 92) then
                state = "backslash"
              elseif (b == 34) then
                state = "done"
              end
            else
              state = "base"
            end
            if (not b or (state == "done")) then
              break
            end
          end
          if not b then
            badend()
          end
          stack[#stack] = nil
          local raw = string.char(unpack(chars))
          local formatted = nil
          local function _2_(c)
            return ("\\" .. c:byte())
          end
          formatted = raw:gsub("[\1-\31]", _2_)
          local load_fn = (_G.loadstring or load)(string.format("return %s", formatted))
          dispatch(load_fn())
        elseif prefixes[b] then
          table.insert(stack, {prefix = prefixes[b]})
          local nextb = getb()
          if whitespace_3f(nextb) then
            if (b ~= 35) then
              parse_error("invalid whitespace after quoting prefix")
            end
            stack[#stack] = nil
            dispatch(utils.sym("#"))
          end
          ungetb(nextb)
        elseif (symbolchar_3f(b) or (b == string.byte("~"))) then
          local chars = {}
          local bytestart = byteindex
          while true do
            chars[(#chars + 1)] = b
            b = getb()
            if (not b or not symbolchar_3f(b)) then
              break
            end
          end
          if b then
            ungetb(b)
          end
          local rawstr = string.char(unpack(chars))
          if (rawstr == "true") then
            dispatch(true)
          elseif (rawstr == "false") then
            dispatch(false)
          elseif (rawstr == "...") then
            dispatch(utils.varg())
          elseif rawstr:match("^:.+$") then
            dispatch(rawstr:sub(2))
          elseif (rawstr:match("^~") and (rawstr ~= "~=")) then
            parse_error("illegal character: ~")
          else
            local force_number = rawstr:match("^%d")
            local number_with_stripped_underscores = rawstr:gsub("_", "")
            local x = nil
            if force_number then
              x = (tonumber(number_with_stripped_underscores) or parse_error(("could not read number \"" .. rawstr .. "\"")))
            else
              x = tonumber(number_with_stripped_underscores)
              if not x then
                if rawstr:match("%.[0-9]") then
                  byteindex = (((byteindex - #rawstr) + rawstr:find("%.[0-9]")) + 1)
                  parse_error(("can't start multisym segment " .. "with a digit: " .. rawstr))
                elseif (rawstr:match("[%.:][%.:]") and (rawstr ~= "..") and (rawstr ~= "$...")) then
                  byteindex = ((byteindex - #rawstr) + 1 + rawstr:find("[%.:][%.:]"))
                  parse_error(("malformed multisym: " .. rawstr))
                elseif rawstr:match(":.+[%.:]") then
                  byteindex = ((byteindex - #rawstr) + rawstr:find(":.+[%.:]"))
                  parse_error(("method must be last component " .. "of multisym: " .. rawstr))
                else
                  x = utils.sym(rawstr, nil, {byteend = byteindex, bytestart = bytestart, filename = filename, line = line})
                end
              end
            end
            dispatch(x)
          end
        else
          parse_error(("illegal character: " .. string.char(b)))
        end
        if done_3f then
          break
        end
      end
      return true, retval
    end
    local function _0_()
      stack = {}
      return nil
    end
    return parse_stream, _0_
  end
  return {["string-stream"] = string_stream, granulate = granulate, parser = parser}
end
local utils = nil
package.preload["fennel.utils"] = package.preload["fennel.utils"] or function(...)
  local function stablepairs(t)
    local keys = {}
    local succ = {}
    for k in pairs(t) do
      table.insert(keys, k)
    end
    local function _0_(a, b)
      return (tostring(a) < tostring(b))
    end
    table.sort(keys, _0_)
    for i, k in ipairs(keys) do
      succ[k] = keys[(i + 1)]
    end
    local function stablenext(tbl, idx)
      if (idx == nil) then
        return keys[1], tbl[keys[1]]
      else
        return succ[idx], tbl[succ[idx]]
      end
    end
    return stablenext, t, nil
  end
  local function map(t, f, out)
    local out0 = (out or {})
    local f0 = nil
    if (type(f) == "function") then
      f0 = f
    else
      local s = f
      local function _0_(x)
        return x[s]
      end
      f0 = _0_
    end
    for _, x in ipairs(t) do
      local _1_0 = f0(x)
      if (nil ~= _1_0) then
        local v = _1_0
        table.insert(out0, v)
      end
    end
    return out0
  end
  local function kvmap(t, f, out)
    local out0 = (out or {})
    local f0 = nil
    if (type(f) == "function") then
      f0 = f
    else
      local s = f
      local function _0_(x)
        return x[s]
      end
      f0 = _0_
    end
    for k, x in stablepairs(t) do
      local korv, v = f0(k, x)
      if (korv and not v) then
        table.insert(out0, korv)
      end
      if (korv and v) then
        out0[korv] = v
      end
    end
    return out0
  end
  local function copy(from, to)
    local to0 = (to or {})
    for k, v in pairs((from or {})) do
      to0[k] = v
    end
    return to0
  end
  local function member_3f(x, tbl, n)
    local _0_0 = tbl[(n or 1)]
    if (_0_0 == x) then
      return true
    elseif (_0_0 == nil) then
      return false
    else
      local _ = _0_0
      return member_3f(x, tbl, ((n or 1) + 1))
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
        local meta = getmetatable(t)
        if (meta and meta.__index) then
          t = meta.__index
          return allpairs_next(t)
        end
      end
    end
    return allpairs_next
  end
  local function deref(self)
    return self[1]
  end
  local nil_sym = nil
  local function list__3estring(self, tostring2)
    local safe, max = {}, 0
    for k in pairs(self) do
      if ((type(k) == "number") and (k > max)) then
        max = k
      end
    end
    for i = 1, max, 1 do
      safe[i] = (((self[i] == nil) and nil_sym) or self[i])
    end
    return ("(" .. table.concat(map(safe, (tostring2 or tostring)), " ", 1, max) .. ")")
  end
  local symbol_mt = {"SYMBOL", __fennelview = deref, __tostring = deref}
  local expr_mt = {"EXPR", __tostring = deref}
  local list_mt = {"LIST", __fennelview = list__3estring, __tostring = list__3estring}
  local sequence_marker = {"SEQUENCE"}
  local vararg = setmetatable({"..."}, {"VARARG", __fennelview = deref, __tostring = deref})
  local getenv = nil
  local function _0_()
    return nil
  end
  getenv = ((os and os.getenv) or _0_)
  local function debug_on_3f(flag)
    local level = (getenv("FENNEL_DEBUG") or "")
    return ((level == "all") or level:find(flag))
  end
  local function list(...)
    return setmetatable({...}, list_mt)
  end
  local function sym(str, scope, source)
    local s = {str, scope = scope}
    for k, v in pairs((source or {})) do
      if (type(k) == "string") then
        s[k] = v
      end
    end
    return setmetatable(s, symbol_mt)
  end
  nil_sym = sym("nil")
  local function sequence(...)
    return setmetatable({...}, {sequence = sequence_marker})
  end
  local function expr(strcode, etype)
    return setmetatable({strcode, type = etype}, expr_mt)
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
  local function table_3f(x)
    return ((type(x) == "table") and (x ~= vararg) and (getmetatable(x) ~= list_mt) and (getmetatable(x) ~= symbol_mt) and x)
  end
  local function sequence_3f(x)
    local mt = ((type(x) == "table") and getmetatable(x))
    return (mt and (mt.sequence == sequence_marker) and x)
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
  local function walk_tree(root, f, custom_iterator)
    local function walk(iterfn, parent, idx, node)
      if f(idx, node, parent) then
        for k, v in iterfn(node) do
          walk(iterfn, node, k, v)
        end
        return nil
      end
    end
    walk((custom_iterator or pairs), nil, nil, root)
    return root
  end
  local lua_keywords = {"and", "break", "do", "else", "elseif", "end", "false", "for", "function", "if", "in", "local", "nil", "not", "or", "repeat", "return", "then", "true", "until", "while"}
  for i, v in ipairs(lua_keywords) do
    lua_keywords[v] = i
  end
  local function valid_lua_identifier_3f(str)
    return (str:match("^[%a_][%w_]*$") and not lua_keywords[str])
  end
  local propagated_options = {"allowedGlobals", "indent", "correlate", "useMetadata", "env", "compiler-env"}
  local function propagate_options(options, subopts)
    for _, name in ipairs(propagated_options) do
      subopts[name] = options[name]
    end
    return subopts
  end
  local root = nil
  local function _1_()
  end
  root = {chunk = nil, options = nil, reset = _1_, scope = nil}
  root["set-reset"] = function(_2_0)
    local _3_ = _2_0
    local chunk = _3_["chunk"]
    local options = _3_["options"]
    local reset = _3_["reset"]
    local scope = _3_["scope"]
    root.reset = function()
      root.chunk, root.scope, root.options, root.reset = chunk, scope, options, reset
      return nil
    end
    return root.reset
  end
  local function hook(event, ...)
    if (root.options and root.options.plugins) then
      for _, plugin in ipairs(root.options.plugins) do
        local _3_0 = plugin[event]
        if (nil ~= _3_0) then
          local f = _3_0
          f(...)
        end
      end
      return nil
    end
  end
  return {["debug-on?"] = debug_on_3f, ["expr?"] = expr_3f, ["list?"] = list_3f, ["lua-keywords"] = lua_keywords, ["member?"] = member_3f, ["multi-sym?"] = multi_sym_3f, ["propagate-options"] = propagate_options, ["quoted?"] = quoted_3f, ["sequence?"] = sequence_3f, ["sym?"] = sym_3f, ["table?"] = table_3f, ["valid-lua-identifier?"] = valid_lua_identifier_3f, ["varg?"] = varg_3f, ["walk-tree"] = walk_tree, allpairs = allpairs, copy = copy, deref = deref, expr = expr, hook = hook, kvmap = kvmap, list = list, map = map, path = table.concat({"./?.fnl", "./?/init.fnl", getenv("FENNEL_PATH")}, ";"), root = root, sequence = sequence, stablepairs = stablepairs, sym = sym, varg = varg}
end
utils = require("fennel.utils")
local parser = require("fennel.parser")
local compiler = require("fennel.compiler")
local specials = require("fennel.specials")
local repl = require("fennel.repl")
local function eval(str, options, ...)
  local opts = utils.copy(options)
  local _ = nil
  if ((opts.allowedGlobals == nil) and not getmetatable(opts.env)) then
    opts.allowedGlobals = specials["current-global-names"](opts.env)
    _ = nil
  else
  _ = nil
  end
  local env = nil
  if (opts.env == "_COMPILER") then
    env = specials["wrap-env"](specials["make-compiler-env"](nil, compiler.scopes.compiler, {}))
  else
    env = (opts.env and specials["wrap-env"](opts.env))
  end
  local lua_source = compiler["compile-string"](str, opts)
  local loader = nil
  local function _2_(...)
    if opts.filename then
      return ("@" .. opts.filename)
    else
      return str
    end
  end
  loader = specials["load-code"](lua_source, env, _2_(...))
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
local mod = {["compile-stream"] = compiler["compile-stream"], ["compile-string"] = compiler["compile-string"], ["list?"] = utils["list?"], ["load-code"] = specials["load-code"], ["macro-loaded"] = specials["macro-loaded"], ["make-searcher"] = specials["make-searcher"], ["search-module"] = specials["search-module"], ["string-stream"] = parser["string-stream"], ["sym?"] = utils["sym?"], compile = compiler.compile, compile1 = compiler.compile1, compileStream = compiler["compile-stream"], compileString = compiler["compile-string"], doc = specials.doc, dofile = dofile_2a, eval = eval, gensym = compiler.gensym, granulate = parser.granulate, list = utils.list, loadCode = specials["load-code"], macroLoaded = specials["macro-loaded"], makeSearcher = specials["make-searcher"], make_searcher = specials["make-searcher"], mangle = compiler["global-mangling"], metadata = compiler.metadata, parser = parser.parser, path = utils.path, repl = repl, scope = compiler["make-scope"], searchModule = specials["search-module"], searcher = specials["make-searcher"](), stringStream = parser["string-stream"], sym = utils.sym, traceback = compiler.traceback, unmangle = compiler["global-unmangling"], varg = utils.varg, version = "0.6.1-dev"}
utils["fennel-module"] = mod
do
  local builtin_macros = [===[;; This module contains all the built-in Fennel macros. Unlike all the other
  ;; modules that are loaded by the old bootstrap compiler, this runs in the
  ;; compiler scope of the version of the compiler being defined.
  
  ;; The code for these macros is somewhat idiosyncratic because it cannot use any
  ;; macros which have not yet been defined.
  
  (fn -> [val ...]
    "Thread-first macro.
  Take the first value and splice it into the second form as its first argument.
  The value of the second form is spliced into the first arg of the third, etc."
    (var x val)
    (each [_ e (ipairs [...])]
      (let [elt (if (list? e) e (list e))]
        (table.insert elt 2 x)
        (set x elt)))
    x)
  
  (fn ->> [val ...]
    "Thread-last macro.
  Same as ->, except splices the value into the last position of each form
  rather than the first."
    (var x val)
    (each [_ e (pairs [...])]
      (let [elt (if (list? e) e (list e))]
        (table.insert elt x)
        (set x elt)))
    x)
  
  (fn -?> [val ...]
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
  
  (fn -?>> [val ...]
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
  
  (fn doto [val ...]
    "Evaluates val and splices it into the first argument of subsequent forms."
    (let [name (gensym)
          form `(let [,name ,val])]
      (each [_ elt (pairs [...])]
        (table.insert elt 2 name)
        (table.insert form elt))
      (table.insert form name)
      form))
  
  (fn when [condition body1 ...]
    "Evaluate body for side-effects only when condition is truthy."
    (assert body1 "expected body")
    `(if ,condition
         (do ,body1 ,...)))
  
  (fn with-open [closable-bindings ...]
    "Like `let`, but invokes (v:close) on each binding after evaluating the body.
  The body is evaluated inside `xpcall` so that bound values will be closed upon
  encountering an error before propagating it."
    (let [bodyfn    `(fn [] ,...)
          closer `(fn close-handlers# [ok# ...] (if ok# ...
                                                    (error ... 0)))
          traceback `(. (or package.loaded.fennel debug) :traceback)]
      (for [i 1 (# closable-bindings) 2]
        (assert (sym? (. closable-bindings i))
                "with-open only allows symbols in bindings")
        (table.insert closer 4 `(: ,(. closable-bindings i) :close)))
      `(let ,closable-bindings ,closer
            (close-handlers# (xpcall ,bodyfn ,traceback)))))
  
  (fn partial [f ...]
    "Returns a function with all arguments partially applied to f."
    (let [body (list f ...)]
      (table.insert body _VARARG)
      `(fn [,_VARARG] ,body)))
  
  (fn pick-args [n f]
    "Creates a function of arity n that applies its arguments to f.
  
  For example,
    (pick-args 2 func)
  expands to
    (fn [_0_ _1_] (func _0_ _1_))"
    (assert (and (= (type n) :number) (= n (math.floor n)) (>= n 0))
            "Expected n to be an integer literal >= 0.")
    (let [bindings []]
      (for [i 1 n] (tset bindings i (gensym)))
      `(fn ,bindings (,f ,(unpack bindings)))))
  
  (fn pick-values [n ...]
    "Like the `values` special, but emits exactly n values.
  
  For example,
    (pick-values 2 ...)
  expands to
    (let [(_0_ _1_) ...]
      (values _0_ _1_))"
    (assert (and (= :number (type n)) (>= n 0) (= n (math.floor n)))
            "Expected n to be an integer >= 0")
    (let [let-syms   (list)
          let-values (if (= 1 (select :# ...)) ... `(values ,...))]
      (for [i 1 n] (table.insert let-syms (gensym)))
      (if (= n 0) `(values)
          `(let [,let-syms ,let-values] (values ,(unpack let-syms))))))
  
  (fn lambda [...]
    "Function literal with arity checking.
  Will throw an exception if a declared argument is passed in as nil, unless
  that argument name begins with ?."
    (let [args [...]
          has-internal-name? (sym? (. args 1))
          arglist (if has-internal-name? (. args 2) (. args 1))
          docstring-position (if has-internal-name? 3 2)
          has-docstring? (and (> (# args) docstring-position)
                              (= :string (type (. args docstring-position))))
          arity-check-position (- 4 (if has-internal-name? 0 1)
                                  (if has-docstring? 0 1))
          empty-body? (< (# args) arity-check-position)]
      (fn check! [a]
        (if (table? a)
            (each [_ a (pairs a)]
              (check! a))
            (and (not (string.match (tostring a) "^?"))
                 (not= (tostring a) "&")
                 (not= (tostring a) "..."))
            (table.insert args arity-check-position
                          `(assert (not= nil ,a)
                                   (string.format "Missing argument %s on %s:%s"
                                                  ,(tostring a)
                                                  ,(or a.filename "unknown")
                                                  ,(or a.line "?"))))))
      (assert (= :table (type arglist)) "expected arg list")
      (each [_ a (ipairs arglist)]
        (check! a))
      (if empty-body?
          (table.insert args (sym :nil)))
      `(fn ,(unpack args))))
  
  (fn macro [name ...]
    "Define a single macro."
    (assert (sym? name) "expected symbol for macro name")
    (local args [...])
    `(macros { ,(tostring name) (fn ,name ,(unpack args))}))
  
  (fn macrodebug [form return?]
    "Print the resulting form after performing macroexpansion.
  With a second argument, returns expanded form as a string instead of printing."
    (let [(ok view) (pcall require :fennelview)
          handle (if return? `do `print)]
      `(,handle ,((if ok view tostring) (macroexpand form _SCOPE)))))
  
  (fn import-macros [binding1 module-name1 ...]
    "Binds a table of macros from each macro module according to a binding form.
  Each binding form can be either a symbol or a k/v destructuring table.
  Example:
    (import-macros mymacros                 :my-macros    ; bind to symbol
                   {:macro1 alias : macro2} :proj.macros) ; import by name"
    (assert (and binding1 module-name1 (= 0 (% (select :# ...) 2)))
            "expected even number of binding/modulename pairs")
    (for [i 1 (select :# binding1 module-name1 ...) 2]
      (local (binding modname) (select i binding1 module-name1 ...))
      ;; generate a subscope of current scope, use require-macros
      ;; to bring in macro module. after that, we just copy the
      ;; macros from subscope to scope.
      (local scope (get-scope))
      (local subscope (fennel.scope scope))
      (fennel.compile-string (string.format "(require-macros %q)"
                                           modname)
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
    nil)
  
  ;;; Pattern matching
  
  (fn match-pattern [vals pattern unifications]
    "Takes the AST of values and a single pattern and returns a condition
  to determine if it matches as well as a list of bindings to
  introduce for the duration of the body if it does match."
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
            (values (if (or wildcard? (string.find (tostring pattern) "^?"))
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
          ;; table patterns
          (= (type pattern) :table)
          (let [condition `(and (= (type ,val) :table))
                bindings []]
            (each [k pat (pairs pattern)]
              (if (and (sym? pat) (= "&" (tostring pat)))
                  (do (assert (not (. pattern (+ k 2)))
                              "expected rest argument before last parameter")
                      (table.insert bindings (. pattern (+ k 1)))
                      (table.insert bindings [`(select ,k ((or _G.unpack
                                                               table.unpack)
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
    "Construct the actual `if` AST for the given match values and clauses."
    (if (not= 0 (% (length clauses) 2)) ; treat odd final clause as default
        (table.insert clauses (length clauses) (sym :_)))
    (let [out `(if)]
      (for [i 1 (length clauses) 2]
        (let [pattern (. clauses i)
              body (. clauses (+ i 1))
              (condition bindings) (match-pattern vals pattern {})]
          (table.insert out condition)
          (table.insert out `(let ,bindings ,body))))
      out))
  
  (fn match-val-syms [clauses]
    "How many multi-valued clauses are there? return a list of that many gensyms."
    (let [syms (list (gensym))]
      (for [i 1 (length clauses) 2]
        (if (list? (. clauses i))
            (each [valnum (ipairs (. clauses i))]
              (if (not (. syms valnum))
                  (tset syms valnum (gensym))))))
      syms))
  
  (fn match [val ...]
    "Perform pattern matching on val. See reference for details."
    (let [clauses [...]
          vals (match-val-syms clauses)]
      ;; protect against multiple evaluation of the value, bind against as
      ;; many values as we ever match against in the clauses.
      (list `let [vals val]
            (match-condition vals clauses))))
  
  {: -> : ->> : -?> : -?>>
   : doto : when : with-open
   : partial : lambda
   : pick-args : pick-values
   : macro : macrodebug : import-macros
   : match}
  ]===]
  local module_name = "fennel.macros"
  local _ = nil
  local function _0_()
    return mod
  end
  package.preload[module_name] = _0_
  _ = nil
  local env = specials["make-compiler-env"](nil, compiler.scopes.compiler, {})
  local _0 = nil
  env.fennel = mod
  _0 = nil
  local built_ins = eval(builtin_macros, {allowedGlobals = false, env = env, filename = "src/fennel/macros.fnl", moduleName = module_name, scope = compiler.scopes.compiler, useMetadata = true})
  for k, v in pairs(built_ins) do
    compiler.scopes.global.macros[k] = v
  end
  compiler.scopes.global.macros["\206\187"] = compiler.scopes.global.macros.lambda
  package.preload[module_name] = nil
end
return mod
