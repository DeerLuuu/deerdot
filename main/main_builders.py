"""Functions used to generate source files during build time"""

import methods


def make_splash(target, source, env):
    buffer = methods.get_buffer(str(source[0]))

    with methods.generated_wrapper(str(target[0])) as file:
        # deer Engine brand cream (#fff4dd). Matches the edge color of the
        # shipped splash artwork so the window background blends seamlessly.
        file.write(f"""\
#include "core/math/color.h"

static const Color boot_splash_bg_color = Color(255 / 255.0, 244 / 255.0, 221 / 255.0);
inline constexpr const unsigned char boot_splash_png[] = {{
	{methods.format_buffer(buffer, 1)}
}};
""")


def make_splash_editor(target, source, env):
    buffer = methods.get_buffer(str(source[0]))

    with methods.generated_wrapper(str(target[0])) as file:
        # deer Engine brand cream (#fff4dd), matching the edge color of the
        # editor splash artwork. Keeps the window background seamless.
        file.write(f"""\
#include "core/math/color.h"

static const Color boot_splash_editor_bg_color = Color(255 / 255.0, 244 / 255.0, 221 / 255.0);
inline constexpr const unsigned char boot_splash_editor_png[] = {{
	{methods.format_buffer(buffer, 1)}
}};
""")


def make_app_icon(target, source, env):
    buffer = methods.get_buffer(str(source[0]))

    with methods.generated_wrapper(str(target[0])) as file:
        # Embed the deer Engine app icon (128x128 PNG, generated from icon.svg).
        file.write(f"""\
inline constexpr const unsigned char app_icon_png[] = {{
	{methods.format_buffer(buffer, 1)}
}};
""")
