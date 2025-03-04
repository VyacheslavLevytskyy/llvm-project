_T = require('lua_lldb_test').create_test('TestFileHandle')

function _T:TestLegacyFileOutScript()
    local f = io.open(self.output, 'w')
    local sbf = lldb.SBFile(f)
    self.debugger:SetOutputFile(sbf)
    self:handle_command('script print(1+1)')
    self.debugger:GetOutputFileHandle():write('FOO\n')
    self.debugger:GetOutputFileHandle():flush()
    f:close()

    f = io.open(self.output, 'r')
    assertEqual(read_file_non_empty_lines(f), {'2', 'FOO'})
    f:close()
end

function _T:TestLegacyFileOut()
    local f = io.open(self.output, 'w')
    local sbf = lldb.SBFile(f)
    self.debugger:SetOutputFile(sbf)
    self:handle_command('expression/x 3735928559', false)
    f:close()

    f = io.open(self.output, 'r')
    assertStrContains(f:read('*l'), 'deadbeef')
    f:close()
end

function _T:TestLegacyFileErr()
    local f = io.open(self.output, 'w')
    local sbf = lldb.SBFile(f)
    self.debugger:SetErrorFile(sbf)
    self:handle_command('lol', false)
    f:close()

    f = io.open(self.output, 'r')
    assertStrContains(f:read('*l'), 'is not a valid command')
    f:close()
end

os.exit(_T:run())
