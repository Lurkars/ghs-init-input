$fa = 1;
$fs = 0.05;
column_width = 14;
column_offset = 4;
number_y = -4;
number_x = -3;
bottom_offset_y = 70 + column_offset;
bottom_offset_z = 0;

difference() {  
    minkowski()
    {
      translate([2.5,2.5,0])
      cube([77,63+column_offset,20.5]);
      cylinder(r=2.5,h=2.5);
    }
    
    translate([1.5,1.5,3])
    cube([79,65+column_offset,30]);
    
    minkowski()
    {
      translate([15+number_x,12+number_y,-1])
      cube([52,42+column_width,5]);
      cylinder(r=2.5,h=5);
    }
      
    translate([11.5+number_x,10+number_y,-1])
    cylinder(r=1.25,h=7);
    translate([11.5+number_x,56+number_y+column_width,-1])
    cylinder(r=1.25,h=7);
    translate([70.5+number_x,10+number_y,-1])
    cylinder(r=1.25,h=7);
    translate([70.5+number_x,56+number_y+column_width,-1])
    cylinder(r=1.25,h=7);
    
    // usb
    translate([63+number_x,64+column_offset,13])
    cube([12,9,7]);
    
    // on-off
    translate([13,63+column_offset,6.75])
    cube([17,7,8]);
   
    translate([11.5,70+column_offset,10.75])
    rotate([90,0,0])
    cylinder(r=1.25,h=6);
    
    translate([31.5,70+column_offset,10.75])
    rotate([90,0,0])
    cylinder(r=1.25,h=6);
    
    // led
    translate([69+number_x,-1,16.5])
    cube([5,5,3]);
}

// bottom holes
    difference() {  
        translate([3,3,18])
        cylinder(r=2.5,h=5);
        translate([3,3,15])
        cylinder(r=1.25,h=10);
    }

    difference() {  
        translate([3,65+column_offset,18])
        cylinder(r=2.5,h=5);
        translate([3,65+column_offset,15])
        cylinder(r=1.25,h=10);
    }

    difference() {  
        translate([79,3,18])
        cylinder(r=2.5,h=5);
        translate([79,3,15])
        cylinder(r=1.25,h=10);
    }

    difference() {  
        translate([79,65+column_offset,18])
        cylinder(r=2.5,h=5);
        translate([79,65+column_offset,15])
        cylinder(r=1.25,h=10);
    }

// bottom
difference() {
    minkowski()
    {
      translate([2.5,2.5+bottom_offset_y,0+bottom_offset_z])
      cube([77,63+column_offset,1]);
      cylinder(r=2.5,h=0.5);
    }

    translate([3,3+bottom_offset_y,-1+bottom_offset_z])
    cylinder(r=1.25,h=10);

    translate([3,65+bottom_offset_y+column_offset,-1+bottom_offset_z])
    cylinder(r=1.25,h=10);

    translate([79,3+bottom_offset_y,-1+bottom_offset_z])
    cylinder(r=1.25,h=10);

    translate([79,65+bottom_offset_y+column_offset,-1+bottom_offset_z])
    cylinder(r=1.25,h=10);
}
