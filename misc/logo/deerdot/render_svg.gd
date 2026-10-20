extends SceneTree

# Headless SVG rasterizer for deer Engine branding assets.
# Usage: godot --headless -s svg2png.gd -- <in.svg> <out.png> <scale>
# (scale = target_width / svg_natural_width)

func _init() -> void:
	var user_args := OS.get_cmdline_user_args()
	if user_args.size() < 3:
		push_error("usage: -- <in.svg> <out.png> <scale>")
		quit(1)
		return
	var svg_bytes := FileAccess.get_file_as_bytes(user_args[0])
	if svg_bytes.is_empty():
		push_error("cannot read svg: " + user_args[0])
		quit(1)
		return
	var scale := float(user_args[2])
	var img := Image.new()
	var err := img.load_svg_from_buffer(svg_bytes, scale)
	if err != OK:
		push_error("svg load failed (%d): %s" % [err, user_args[0]])
		quit(1)
		return
	err = img.save_png(user_args[1])
	if err != OK:
		push_error("png save failed (%d): %s" % [err, user_args[1]])
		quit(1)
		return
	print("rasterized %s -> %s (%dx%d)" % [user_args[0], user_args[1], img.get_width(), img.get_height()])
	quit(0)
