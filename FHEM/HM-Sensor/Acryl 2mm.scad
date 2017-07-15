// variables
pH=20;  // gesamt
kH=6;   // kabel

//
module roundedEdge(x=0, y=0, z=-1, a=0, l=30, h=pH+2) 
{
    translate([x, y, z]) rotate(a)
    difference() {
        translate([0, 0, 0])
        linear_extrude(h) 
        polygon([[0, 0], [l/2, 0], [0, l/2]]);
        translate([l/2, l/2, 0]) cylinder(r=l/2, h=h+2);
    }
}

scale([0.1, 0.1, 0.1])
difference() {
translate([0, 0, -1])
cube([300, 1200, pH], center=false);

// befestigung
translate([150, 100, -1])
cylinder(r=50, h=pH+2, center=false);
translate([150, 540, -1])
cylinder(r=50, h=pH+2, center=false);
// vierkant
translate([150, 320, -1])
cylinder(r=90, h=pH+2, center=false);
// reed
translate([242, 270, -1])
cube([22, 200, pH+2], center=false);
translate([36, 270, -1])
cube([22, 200, pH+2], center=false);

// platine
translate([30, 710, 14])
cube([61, 61, 7], center=false);
translate([50, 730, 6])
cube([41, 41, 15], center=false);
translate([90, 710, -1])
cube([180, 61, pH+2], center=false);

translate([30, 770, -1])
cube([240, 340, pH+2], center=false);

translate([210, 1109, 14])
cube([61, 61, 7], center=false);
translate([210, 1109, 6])
cube([41, 41, 15], center=false);
translate([30, 1109, -1])
cube([181, 61, pH+2], center=false);

// kabel
translate([250, 469, -1])
cube([kH, 242, kH], center=false);
translate([46, 469, -1])
cube([210, kH, kH], center=false);
roundedEdge(x=251, y=474, h=6, a=90);

// antenne    
translate([25, 355, -1])
cube([kH, 422, kH], center=false);

roundedEdge();
roundedEdge(x=300, y=-1, a=90);
roundedEdge(x=301, y=1200, a=180);
roundedEdge(y=1201, a=270);
}

dH=80;  // gesamt

scale([0.1, 0.1, 0.1])
translate([0, 0, -1])
difference() {
translate([400, 680, 0])
cube([300, 520, dH], center=false);

translate([609, 710, 64])
cube([61, 61, 17], center=false);
translate([430, 710, 11])
cube([180, 61, dH+2], center=false);

translate([430, 770, 11])
cube([240, 340, dH+2], center=false);

translate([430, 1109, 64])
cube([61, 61, 17], center=false);
translate([490, 1109, 11])
cube([180, 61, dH+2], center=false);

roundedEdge(x=400, y=1201, a=270, h=dH);
roundedEdge(x=701, y=1200, a=180, h=dH);
    
translate([700, 1200, -2]) rotate([90, 0, 270]) roundedEdge(x=0, y=0, h=300);
translate([399, 679, -2]) rotate([90, 270, 180]) roundedEdge(x=0, y=0, h=522);
translate([700, 679, -2]) rotate([90, 0, 180]) roundedEdge(x=0, y=0, h=522);
}
