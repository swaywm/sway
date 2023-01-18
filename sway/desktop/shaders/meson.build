embed = find_program('./embed.sh', native: true)

shaders = [
    'common.vert',
    'quad.frag',
    'quad_round.frag',
    'quad_round_tl.frag',
    'quad_round_tr.frag',
    'corner.frag',
    'box_shadow.frag',
    'tex_rgba.frag',
    'tex_rgbx.frag',
    'tex_external.frag',
]

foreach name : shaders
    output = name.underscorify() + '_src.h'
    var = name.underscorify() + '_src'
    sway_sources += custom_target(
        output,
        command: [embed, var],
        input: name,
        output: output,
        feed: true,
        capture: true,
    )
endforeach
