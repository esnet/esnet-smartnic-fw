*** Settings ***
Documentation     Setup the device under test for register accesses.
Library           fixtures.Library
Suite Setup       Device Start
Suite Teardown    Device Stop
