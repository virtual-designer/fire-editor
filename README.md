    while ((c = getch()) != 'q') {
        switch (c) {
            case KEY_UP:
                if (config.offset > 0 && config.cury < 2) {
                    config.offset--;
                    fire_render_rows();
                    move(config.cury, config.curx);
                }
                else if (config.cury > 1) {
                    move(--config.cury, config.curx);
                }
            break;

            case KEY_DOWN:
                if (config.cury >= (config.rows - 1) && config.offset <= (config.line_count + 1)) {
                    config.offset++;
                    fire_render_rows();
                    move(config.cury, config.curx);
                }
                else if ((config.cury + config.offset) <= (config.line_count - 1)) {
                    move(++config.cury, config.curx);
                }
            break;

            case KEY_LEFT:

            break;
            case KEY_RIGHT:

            