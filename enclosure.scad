// Arduino HomeMonitor Enclosure v3
// Front panel : LCD (centered, padded) + button + buzzer sound hole
// Left wall   : power switch + DC barrel + USB (all Arduino cables on one side)
// No screws   : friction-fit lid (rim slides into the box opening)
//
// Units: mm. +0.35mm tolerance on all cutouts.

$fn = 72;

outer = [115, 90, 50];          // L x W x H
wall  = 2.5;                    // wall thickness
inner = [outer[0]-2*wall, outer[1]-2*wall, outer[2]-wall]; // 110 x 85 x 47.5
tol   = 0.35;                   // print tolerance added to cutouts
BOX_H = 45;                     // bottom box height (walls); lid fills 45..50

/* ---------- base shapes ---------- */
module shell() {
    difference() {
        cube(outer);
        translate([wall, wall, wall]) cube(inner);
    }
}
module bottom_box() {
    intersection() { shell(); cube([outer[0], outer[1], BOX_H]); }
}
module top_lid() {
    translate([0, 0, BOX_H]) {
        // locating lip (slides inside the box opening)
        translate([wall + 0.25, wall + 0.25, 0])
            cube([inner[0] - 0.5, inner[1] - 0.5, outer[2]-BOX_H-wall]);
        // top plate
        translate([0, 0, outer[2]-BOX_H-wall])
            cube([outer[0], outer[1], wall]);
    }
}

/* ---------- front panel cutouts (Y=0 face) ---------- */
// LCD window 71.5 x 24.5, centered, vertical centre Z=25
LCD_W = 71.5; LCD_H = 24.5;
LCD_X = (outer[0] - LCD_W) / 2;      // 21.75
LCD_Z = 25 - LCD_H/2;                // 12.75
BUTTON = [107, 25, 7.2];             // x, z, diameter

module front_hole(x, z, d) {
    translate([x, wall+2, z]) rotate([90,0,0]) cylinder(d=d, h=wall+2);
}
module front_cutouts() {
    translate([LCD_X, -1, LCD_Z]) cube([LCD_W+tol, wall+2, LCD_H+tol]);
    front_hole(BUTTON[0], BUTTON[1], BUTTON[2]+tol);
}

/* ---------- left wall cutouts (X=0 face) ---------- */
// switch 6.5 x 8 (vertically wider), DC barrel 11 circle, USB 12.5 x 11.5
SWITCH = [20, 32, 6.5, 8.0];         // y-centre, z-centre, y-width, z-height
DC     = [36, 14, 11.0];             // y-centre, z-centre, diameter
USB    = [63, 16.5, 12.5, 11.5];     // y-centre, z-centre, y-width, z-height

module left_cutouts() {
    // power switch (rectangular)
    translate([-1, SWITCH[0]-SWITCH[2]/2, SWITCH[1]-SWITCH[3]/2])
        cube([wall+2, SWITCH[2]+tol, SWITCH[3]+tol]);
    // DC power barrel (round)
    translate([0, DC[0], DC[1]]) rotate([0,90,0]) cylinder(d=DC[2]+tol, h=wall+2);
    // USB (rectangular)
    translate([-1, USB[0]-USB[2]/2, USB[1]-USB[3]/2])
        cube([wall+2, USB[2]+tol, USB[3]+tol]);
}

/* ---------- Arduino Uno ---------- */
UNO = [68.6, 53.4, 2.0];
uno_x = wall + (inner[0] - UNO[0]) / 2;   // 23.2 (centred)
uno_y = 25;                               // front edge, clears LCD depth
uno_z = wall + 5;                         // board top (5mm standoff)

module uno_board() {
    translate([uno_x, uno_y, uno_z]) cube(UNO);
}
module standoffs() {
    for (hx = [14.0, 66.0])
        for (hy = [2.5, 50.8])
            translate([uno_x + hx, uno_y + hy, wall])
                difference() {
                    cylinder(d=5.8, h=5);
                    translate([0,0,-1]) cylinder(d=2.8+tol, h=7);
                }
}

/* ---------- LCD module (for visualisation) ---------- */
module lcd_body() {
    translate([(outer[0]-80)/2, wall, 25-18]) cube([80, 8, 36]);
}

/* ---------- piezo buzzer ring clip (front inner wall) ---------- */
module buzzer_clip() {
    // removed — no buzzer sound hole on the face
}

/* ---------- assembled view ---------- */
module assembled() {
    color("dimgray")   bottom_box();
    color("slategray") top_lid();
    color("darkgreen") uno_board();
    color("silver")    standoffs();
    color("white")     lcd_body();
}

/* ---------- printable parts ---------- */
module print_bottom() {
    difference() {
        union() {
            bottom_box();
            standoffs();
        }
        front_cutouts();
        left_cutouts();
    }
}
module print_top() {
    top_lid();
}

assembled();
