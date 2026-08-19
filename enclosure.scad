// Arduino HomeMonitor Enclosure v5 (Optimized Sleek Desktop Edition)
// - Dimensions: 115mm (L) x 60mm (W) x 68mm (H) [Flat, compact & material-optimized]
// - Wall thickness: 2.0mm (saves ~25% filament while remaining rigid)
// - Modern rounded outer vertical corners (r = 4mm)
// - Front: Vertically & horizontally centered LCD window + 5mm (0.5cm) top pin-clearance notch
// - Left: Single clean raised USB cutout for the vertical Mega
// - Bottom: Lightweight rear rail guide for vertical Mega board

$fn = 60;

/* ===== ENCLOSURE DIMENSIONS ===== */
outer_x = 115.0;
outer_y = 60.0;                 // Slim & flat 60mm depth
outer_z = 68.0;                 // 68mm compact height (clears 53.4mm Mega vertical)
wall    = 2.0;                  // Optimized 2.0mm wall thickness
corner_r = 4.0;                 // Rounded corners for modern aesthetic
tol     = 0.35;                 // Print tolerance
BOX_H   = 64.0;                 // Bottom shell height (Lid = 4.0mm)

/* ===== HELPER: ROUNDED RECTANGLE CUBE ===== */
module rounded_box(dim, r) {
    x = dim[0]; y = dim[1]; z = dim[2];
    hull() {
        translate([r, r, 0]) cylinder(r=r, h=z);
        translate([x - r, r, 0]) cylinder(r=r, h=z);
        translate([r, y - r, 0]) cylinder(r=r, h=z);
        translate([x - r, y - r, 0]) cylinder(r=r, h=z);
    }
}

/* ===== BASE SHELL ===== */
module shell() {
    difference() {
        rounded_box([outer_x, outer_y, outer_z], corner_r);
        // Hollow interior
        translate([wall, wall, wall])
            rounded_box([outer_x - 2*wall, outer_y - 2*wall, outer_z], max(1.0, corner_r - wall));
    }
}

module bottom_box() {
    intersection() {
        shell();
        cube([outer_x, outer_y, BOX_H]);
    }
}

module top_lid() {
    inner_x = outer_x - 2*wall;
    inner_y = outer_y - 2*wall;
    inner_r = max(1.0, corner_r - wall);

    translate([0, 0, BOX_H]) {
        // Inner locating rim (slides into box opening)
        translate([wall + 0.25, wall + 0.25, 0])
            rounded_box([inner_x - 0.5, inner_y - 0.5, outer_z - BOX_H - wall], inner_r);
        // Top plate
        translate([0, 0, outer_z - BOX_H - wall])
            rounded_box([outer_x, outer_y, wall], corner_r);
    }
}

/* ===== FRONT PANEL CUTOUTS (Y = 0) ===== */
LCD_W = 71.5;
LCD_H = 24.5;
LCD_X = (outer_x - LCD_W) / 2;       // 21.75 mm (centered in X)
LCD_Z = (BOX_H - LCD_H) / 2;         // 19.75 mm (centered in Z)

BUTTON_X = 104.0;
BUTTON_Z = LCD_Z + LCD_H / 2;         // Aligned with LCD center
BUTTON_D = 7.2;

// Pin header notch: 48mm wide, exactly 5mm (0.5cm) down from top rim
NOTCH_W = 48.0;
NOTCH_DEPTH = 5.0;                   // 5mm notch as requested

module front_cutouts() {
    // 1. LCD window
    translate([LCD_X, -1, LCD_Z])
        cube([LCD_W + tol, wall + 2, LCD_H + tol]);

    // 2. 5mm (0.5cm) pin-header notch at top rim
    translate([LCD_X, -1, BOX_H - NOTCH_DEPTH])
        cube([NOTCH_W + tol, wall + 2, NOTCH_DEPTH + 2]);

    // 3. Panel button hole
    translate([BUTTON_X, wall + 2, BUTTON_Z])
        rotate([90, 0, 0])
        cylinder(d=BUTTON_D + tol, h=wall + 4);
}

/* ===== LEFT WALL CUTOUTS (X = 0) ===== */
// Single USB port aligned with vertical Mega in the rear
USB_Y = 44.0;
USB_Z = 40.0;
USB_W = 14.0;
USB_H = 12.0;

module left_cutouts() {
    translate([-1, USB_Y - USB_W/2, USB_Z - USB_H/2])
        cube([wall + 2, USB_W + tol, USB_H + tol]);
}

/* ===== LIGHTWEIGHT MEGA GUIDE RAIL ===== */
module board_guide() {
    // Slim 1.5mm vertical guide rails on the floor
    translate([wall + 6, outer_y - wall - 7, wall])
        difference() {
            cube([outer_x - 2*wall - 12, 5.0, 4.0]);
            translate([-1, 1.5, 1.5])
                cube([outer_x - 2*wall - 10, 2.0 + tol, 3.0]);
        }
}

/* ===== ASSEMBLY ===== */
module assembled() {
    color("DimGray", 0.9) bottom_box();
    color("SlateGray", 0.85) top_lid();

    // Visual: LCD module (Green)
    color("DarkGreen", 0.7)
        translate([(outer_x - 80)/2, wall, LCD_Z - 5])
        cube([80, 9, 36]);

    // Visual: Vertical Mega (Blue)
    color("DodgerBlue", 0.7)
        translate([wall + 4, outer_y - wall - 6, wall + 2])
        cube([101.6, 2.0, 53.3]);
}

/* ===== EXPORTS ===== */
module print_bottom() {
    difference() {
        union() {
            bottom_box();
            board_guide();
        }
        front_cutouts();
        left_cutouts();
    }
}

module print_top() {
    top_lid();
}

assembled();
