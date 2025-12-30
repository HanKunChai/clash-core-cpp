#!/bin/bash

# 检查 python3
if ! command -v python3 &> /dev/null; then
    echo "Error: python3 could not be found"
    exit 1
fi

# 检查 PyYAML
python3 -c "import yaml" 2>/dev/null
if [ $? -ne 0 ]; then
    echo "PyYAML not found. Installing..."
    pip3 install pyyaml
fi

SUBSCRIPTION_URL="$1"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TEMPLATE_FILE="$SCRIPT_DIR/example.yml"
OUTPUT_FILE="config.yaml"

if [ ! -f "$TEMPLATE_FILE" ]; then
    echo "Error: Template file $TEMPLATE_FILE not found."
    exit 1
fi

echo "Processing YAML using $TEMPLATE_FILE as source..."
python3 -c "
import yaml
import sys

def load_yaml(path):
    with open(path, 'r', encoding='utf-8') as f:
        return yaml.safe_load(f)

try:
    # Load Template (which is also the source)
    with open('$TEMPLATE_FILE', 'r', encoding='utf-8') as f:
        template = yaml.safe_load(f)
    
    # Use template as sub
    sub = template

    if not sub:
        print('Error: Subscription file is invalid or empty.')
        sys.exit(1)

    # Handle case where subscription is just a list of proxies (rare but possible)
    # or standard clash config structure
    new_proxies = []
    if 'proxies' in sub:
        new_proxies = sub['proxies']
    elif isinstance(sub, list):
        # Sometimes it's just a list of proxies? Unlikely for valid yaml sub, usually it has 'proxies' key.
        # But let's assume standard structure.
        pass
    
    if not new_proxies:
        print('Error: No proxies found in subscription file.')
        sys.exit(1)

    proxy_names = [p['name'] for p in new_proxies]

    # Update proxies in template
    template['proxies'] = new_proxies

    # Update proxy-groups
    if 'proxy-groups' in template:
        for group in template['proxy-groups']:
            if 'proxies' not in group or group['proxies'] is None:
                group['proxies'] = []
            
            # Add new proxies to the group
            # We append all proxies to every group for simplicity, 
            # as most groups in the template seem to be selectors or url-tests that need candidates.
            # You can customize this logic if needed.
            existing_proxies = set(group['proxies'])
            for name in proxy_names:
                if name not in existing_proxies:
                    group['proxies'].append(name)

    # Output to file
    with open('$OUTPUT_FILE', 'w', encoding='utf-8') as f:
        # allow_unicode=True to keep Chinese characters
        yaml.dump(template, f, allow_unicode=True, sort_keys=False, default_flow_style=False)

    print('Successfully generated $OUTPUT_FILE')

except Exception as e:
    print(f'Error processing YAML: {e}')
    sys.exit(1)
"
