<?php
// TODO(iclelland): Generate this sample token during the build. The token
// below will expire in 2033, but it would be better to always have a token which
// is guaranteed to be valid when the tests are run.
// Generate this token with the command:
// generate_token.py http://127.0.0.1:8000 Frobulate -expire-timestamp=2000000000
header("Origin-Trial: AlCoOPbezqtrGMzSzbLQC4c+oPqO6yuioemcBPjgcXajF8jtmZr4B8tJRPAARPbsX6hDeVyXCKHzEJfpBXvZgQEAAABReyJvcmlnaW4iOiAiaHR0cDovLzEyNy4wLjAuMTo4MDAwIiwgImZlYXR1cmUiOiAiRnJvYnVsYXRlIiwgImV4cGlyeSI6IDIwMDAwMDAwMDB9");
?>
<!DOCTYPE html>
<meta charset="utf-8">
<title>Test Sample API when trial is enabled</title>
<script src="../resources/testharness.js"></script>
<script src="../resources/testharness-helpers.js"></script>
<script src="../resources/testharnessreport.js"></script>
<script src="resources/origin_trials.js"></script>
<script>

// The trial is enabled by the token above in header.
expect_success();

</script>
