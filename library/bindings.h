/* In mousepaint_m mode these override the default bindings.
   Drawing with mouse does not affect the original file.
   A new file is created if changes are saved. */
static Binding keydown_bindings_mousepaint_m[] = {
    /* Select the value to write to memory when drawing with mouse.
       The value is passed on the command line.

       Alternatively, the filename of a shared object file (*.so) can be passed.
       This should contain a function with the following prototype:
       void* function(void* in, void* out);
       $in is a pointer to the value under the mouse and
       $out is a pointer to the memory where the new value is to be written.
       The function should return $out.
       If the file is in the current folder, write ./file.so instead of file.so. */
    { SDLK_SPACE,	0,		mp_set_action,			},
    /* Save the current dataset with all changes as a new file.
       The default filename contains the Unix time. */
    { SDLK_RETURN,	0,		mp_save,			},
    { SDLK_KP_ENTER,	0,		mp_save,			},
    /* Save only the current frame; not other variables or timesteps. */
    { SDLK_RETURN,	KMOD_SHIFT,	mp_save_frame,			},
    { SDLK_KP_ENTER,	KMOD_SHIFT,	mp_save_frame,			},
    { SDLK_s,		0,		mp_set_filename,		},
    /* Make the pen bigger or smaller. */
    { SDLK_PLUS,	0,		mp_size,		{.i=1}  },
    { SDLK_MINUS,	0,		mp_size,		{.i=-1} },
};

/* In variables_m mode these override the default bindings.
   In that mode, names of all variables are shown in the command line.
   The following keybindings are used to choose a variable to switch to. */
static Binding keydown_bindings_variables_m[] = {
    { SDLK_UP,		0,			pending_var_dec, },
    { SDLK_v,		KMOD_ALT|KMOD_SHIFT,	pending_var_dec, },
    { SDLK_DOWN,	0,			pending_var_inc, },
    { SDLK_v,		KMOD_ALT,		pending_var_inc, },
    { SDLK_RETURN,	0,			use_and_exit,    },
    { SDLK_KP_ENTER,	0,			use_and_exit,    },
};

/* In colormaps_m mode these override the default bindings.
   In that mode, names of all colormaps are shown in the command line.
   The following keybindings are used to choose a colormap to switch to. */
static Binding keydown_bindings_colormaps_m[] = {
    { SDLK_UP,		0,			pending_map_dec,	},
    { SDLK_c,		KMOD_ALT|KMOD_SHIFT,	pending_map_dec,	},
    { SDLK_DOWN,	0,			pending_map_inc,	},
    { SDLK_c,		KMOD_ALT,		pending_map_inc,	},
    { SDLK_RETURN,	0,			use_map_and_exit,	},
    { SDLK_KP_ENTER,	0,			use_map_and_exit,	},
};

static Binding keydown_bindings[] = {
    { SDLK_q,		0,			quit,					},
    { SDLK_h,		0,			show_bindings,				}, // show this file
    { SDLK_v,		0,			var_ichange,	{.i=1}			}, // Next variable.
    { SDLK_v,		KMOD_SHIFT,		var_ichange,	{.i=-1}			}, // Previous variable.
    { SDLK_v,		KMOD_ALT,		set_prog_mode,	{.i=variables_m}	}, // see keydown_bindings_variables_m for info
    { SDLK_w,		0,			use_lastvar,				}, // switch to that variable which was shown previously
    /* Whether globals such as colormap, invert_y, etc. are detached from other variables. */
    { SDLK_d,		0,			toggle_detached,			},
    /* Colorscale adjustment.
       For example, if a few values are much higher than other, one may want to make the largest value smaller. */
    { SDLK_1,		0,			shift_min,	{.f=-0.02}		}, // make the smallest value smaller
    { SDLK_1,		KMOD_SHIFT,		shift_max,	{.f=-0.02}		}, // make the largest value smaller
    { SDLK_2,		0,			shift_min,	{.f=0.02}		}, // make the smallest value larger
    { SDLK_2,		KMOD_SHIFT,		shift_max,	{.f=0.02}		}, // make the largest value larger
    { SDLK_0,		0,			toggle_var,	{.v=&update_minmax_cur}	}, // scale colors based on the current frame
    /* Shift the colorscale as above but different step size. */
    { SDLK_1,		KMOD_ALT,		shift_min_abs,	{.f=-0.02}		},
    { SDLK_1,		KMOD_SHIFT|KMOD_ALT,	shift_max_abs,	{.f=-0.02}		},
    { SDLK_2,		KMOD_ALT,		shift_min_abs,	{.f=0.02}		},
    { SDLK_2,		KMOD_SHIFT|KMOD_ALT,	shift_max_abs,	{.f=0.02}		},
    /* Choosing the colormap. */
    { SDLK_c,		0,			cmap_ichange,	{.i=1}			}, // Next colormap.
    { SDLK_c,		KMOD_SHIFT,		cmap_ichange,	{.i=-1}			}, // Previous colormap.
    { SDLK_c,		KMOD_ALT,		toggle_var,	{.v=&globs.invert_c}	}, // Invert (reverse) the colormap.
    { SDLK_c,		KMOD_SHIFT|KMOD_ALT,	set_prog_mode,	{.i=colormaps_m}	}, // see keydown_bindings_colormaps_m
    /* Swap foreground and background colors. */
    { SDLK_i,		KMOD_SHIFT,		invert_colors,				},
    /* Print information about the dataset which is being plotted. */
    { SDLK_p,		0,			print_var,				},
    /* Toggle whether information such as the current variable is printed into the terminal. */
    { SDLK_e,		0,			toggle_var,	{.v=&globs.echo}	},
    /* If enabled, virtual pixels correspond exactly to data. This disables the chance for stepless zoom. */
    { SDLK_e,		KMOD_SHIFT,		toggle_var,	{.v=&globs.exact}	},
    /* Toggle whether the largest y-coordinate is in the top or in the bottom. */
    { SDLK_i,		0,			toggle_var,	{.v=&globs.invert_y}	},
    /* Play a video, if the variable has third dimension. Use set_sleep to control the speed. */
    { SDLK_SPACE,	0,			toggle_var,	{.v=&play_on}		},
    { SDLK_SPACE,	KMOD_SHIFT,		toggle_var,	{.v=&play_inv}		},
    /* Jump to some frame, if data has third dimension.
       The framenumber will be passed on the terminal. */
    { SDLK_j,		0,			jump_to,				},
    /* Show or hide coastlines.
       By default, the fastet changing coordinates are handled as longitudes
       and the second fastest changing as latitudes.
       For other options, see ask_crs. */
    { SDLK_l,		0,			toggle_var,	{.v=&globs.coastlines}	},
    /* In mousepaint mode, the file can be edited by drawing to it with mouse.
       Further documentation and the keybindings are listed in keydown_bindings_mousepaint_m. */
    { SDLK_m,		0,			set_prog_mode,	{.i=mousepaint_m}	},
    /* Set a value which will handled such as NAN values.
       The value which to use will be asked in terminal. */
    { SDLK_n,		0,			set_nan,				},
    /* Toggle whether there is a spesific fill value which is handled such as NAN values. */
    { SDLK_n,		KMOD_SHIFT,		toggle_var,	{.v=&globs.usenan}	},
    /* Toggle whether the figure fills the whole window
       meaning that all data are not seen unless the aspect ratio is exactly right. */
    { SDLK_f,		0,			toggle_var,	{.v=&fill_on}		},
    { SDLK_PLUS,	0,			multiply_zoom,	{.f=0.85}		}, // smaller number is more zoom
    { SDLK_MINUS,	0,			multiply_zoom,	{.f=1/0.85}		}, // larger number is less zoom
    { SDLK_RIGHT,	0,			inc_znum,	{.i=1}			},
    { SDLK_LEFT,	0,			inc_znum,	{.i=-1}			},
    /* Move the zoom/view rectangle 7 steps. */
    { SDLK_RIGHT,	KMOD_SHIFT,		inc_offset_i,	{.i=7}			},
    { SDLK_LEFT,	KMOD_SHIFT,		inc_offset_i,	{.i=-7}			},
    { SDLK_UP,		KMOD_SHIFT,		inc_offset_j,	{.i=-7}			},
    { SDLK_DOWN,	KMOD_SHIFT,		inc_offset_j,	{.i=7}			},
    /* Move the zoom/view rectangle 1 step. */
    { SDLK_RIGHT,	KMOD_SHIFT|KMOD_ALT,	inc_offset_i,	{.i=1}			},
    { SDLK_LEFT,	KMOD_SHIFT|KMOD_ALT,	inc_offset_i,	{.i=-1}			},
    { SDLK_UP,		KMOD_SHIFT|KMOD_ALT,	inc_offset_j,	{.i=-1}			},
    { SDLK_DOWN,	KMOD_SHIFT|KMOD_ALT,	inc_offset_j,	{.i=1}			},
    /* Set, how many milliseconds the program sleeps after each event loop.
       The number will be passed on the terminal.
       Higher number makes time series go slower.
       If sleep time is zero, one core will be running 100 % CPU usage. */
    { SDLK_s,		0,			set_sleep,				},
    { SDLK_RETURN,	0,			use_pending,				},
    { SDLK_KP_ENTER,	0,			use_pending,				},
    { SDLK_ESCAPE,	0,			end_curses,				},
    /* Set the coordinate system of this variable.
       The coordinate system will is passed on the terminal.
       If proj-library is available, this will affect how the coastlines are drawn.
       Otherwise this is useless. */
    { SDLK_t,		KMOD_SHIFT,		ask_crs,				},
#ifdef HAVE_NCTPROJ
    /* Convert this variable into different coordinates.
       The original and target coordinate systems will be asked in terminal.
       See e.g. 'man proj' for how to express the coordinate systems. */
    { SDLK_t,		0,			convert_coord,				},
#endif
};
