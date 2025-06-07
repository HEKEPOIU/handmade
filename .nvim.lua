require("overseer").register_template({
    name = "build",
    params = {},
    condition = {
        -- This makes the template only available in the current directory
        -- In case you :cd out later
        dir = vim.fn.getcwd(),
    },
    builder = function()
        return {
            cmd = { "./code/build.bat", },
            cwd = "./code",
            components = {
                "on_exit_set_status",
                { "on_complete_notify" },
                { "on_output_parse",   problem_matcher = "$msCompile" },
                { "open_output",       direction = "vertical", on_start= "never", on_complete= 'failure' },
            },
            args = {},
        }
    end,
})

vim.keymap.set("n", "<A-m>", ":OverseerRun build<CR>")
