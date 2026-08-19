// Arduino HomeMonitor Enclosure v4 (Mega Vertical Landscape Edition)
// - Footprint: 115mm (L) x 70mm (W) x 75mm (H) [Depth reduced by 20mm, Height increased by 25mm]
// - Front face: Centered LCD cutout + pin header notch directly above screen + button hole
// - Left wall: Single USB/Power cutout raised by 20mm (switch and DC barrel removed)
// - 2-piece friction-fit enclosure

$fn = 72;

/* ===== ENCLOSURE DIMENSIONS ===== */
outer = [115, 70, 75];          // 115mm L x 70mm W x 75mm H
wall  = 2.5;                    // 2.5mm wall thickness
inner = [outer[0] - 2*wall, outer[1] - 2*wall, outer[2] - wall]; // 110 x 65 x 72.5
tol   = 0.4;                    // 3D print tolerance
BOX_H = 70;                     // Bottom box height; Lid adds top 5mm

/* ===== BASE SHELL ===== */
module shell() {
    difference() {
        cube(outer);
        translate([wall, wall, wall]) cube(inner);
    }
}

module bottom_box() {
    intersection() {
        shell();
        cube([outer[0], outer[1], BOX_H]);
    }
}

module top_lid() {
    translate([0, 0, BOX_H]) {
        // Locating inner lip (slides cleanly inside the box opening)
        translate([wall + 0.3, wall + 0.3, 0])
            cube([inner[0] - 0.6, inner[1] - 0.6, outer[2] - BOX_H - wall]);
        // Top plate
        translate([0, 0, outer[2] - BOX_H - wall])
            cube([outer[0], outer[1], wall]);
    }
}

/* ===== FRONT PANEL CUTOUTS (Y = 0) ===== */
LCD_W = 71.5;
LCD_H = 24.5;
LCD_X = (outer[0] - LCD_W) / 2;      // 21.75 mm (centered horizontally)
LCD_Z = 16.0;                        // Bottom of LCD window

BUTTON_X = 104.0;
BUTTON_Z = LCD_Z + LCD_H / 2;        // Centered with LCD vertically
BUTTON_D = 7.2;

// Pin header notch: 48mm wide, starts at LCD left edge, cuts out top wall above screen
NOTCH_W = 48.0;

module front_cutouts() {
    // 1. LCD display window
    translate([LCD_X, -1, LCD_Z])
        cube([LCD_W + tol, wall + 2, LCD_H + tol]);
    
    // 2. Pin header notch (unfilled border directly above screen for protruding pins)
    translate([LCD_X - 1, -1, LCD_Z + LCD_H])
        cube([NOTCH_W + tol, wall + 2, BOX_H - (LCD_Z + LCD_H) + 2]);

    // 3. Button hole
    translate([BUTTON_X, wall + 2, BUTTON_Z])
        rotate([90, 0, 0])
        cylinder(d=BUTTON_D + tol, h=wall + 4);
}

/* ===== LEFT WALL CUTOUTS (X = 0) ===== */
// Single USB/Power port (raised by 20mm, extra switch/barrel holes removed)
USB_Y = 50.0;                       // Positioned towards the rear where Mega vertical board sits
USB_Z = 45.0;                       // Raised to align with vertical Mega USB port
USB_W = 15.0;                       // Generous 15mm width for USB plug strain relief
USB_H = 14.0;                       // 14mm height

module left_cutouts() {
    translate([-1, USB_Y - USB_W/2, USB_Z - USB_H/2])
        cube([wall + 2, USB_W + tol, USB_H + tol]);
}

/* ===== MEGA VERTICAL REAR SUPPORT RAIL ===== */
// Subtle internal bottom guides to hold Mega vertical
module board_guide() {
    // Rear bottom slot guide for PCB edge
    translate([wall + 5, outer[1] - wall - 8, wall])
        difference() {
            cube([outer[0] - 2*wall - 10, 6, 6]);
            translate([-1, 2, 2]) cube([outer[0] - 2*wall - 6, 2.5 + tol, 5]);
        }
}

/* ===== ASSEMBLY / PREVIEW ===== */
module assembled() {
    color("DimGray", 0.9) bottom_box();
    color("SlateGray", 0.85) top_lid();
    
    // Visualization of LCD panel (Green)
    color("DarkGreen", 0.7)
        translate([(outer[0] - 80)/2, wall, LCD_Z - 6])
        cube([80, 10, 36]);

    // Visualization of Vertical Arduino Mega (Blue)
    color("DodgerBlue", 0.7)
        translate([wall + 4, outer[1] - wall - 7, wall + 2])
        cube([101.6, 2.0, 53.3]);
}

/* ===== PRINTABLE EXPORTS ===== */
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
