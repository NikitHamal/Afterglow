import marimo._code_mode as cm
async with cm.get_context() as ctx:
    for c in ctx.cells.values():
        print(c.id, c.status, c.errors, c.code.splitlines()[0] if c.code else 'empty')
