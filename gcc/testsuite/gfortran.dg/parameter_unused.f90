! { dg-do compile }
! { dg-options "-Wunused" }
!
! PR fortran/31129 - No warning on unused parameters
!
program fred
integer,parameter :: j = 9     ! { dg-warning "unused parameter" }
end

